#include "mqtt_client.h"
#include "ecc608.h"
#include "flash_string.h"
#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "security_profile.h"
#include "sequans_controller.h"

#include <avr/pgmspace.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_PUBLISH_URC_LENGTH   (32)
#define MQTT_SUBSCRIBE_URC_LENGTH (164)

#define MQTT_MSG_MAX_BUFFER_SIZE    (1024) // This is a limitation from the modem
#define MQTT_MSG_LENGTH_BUFFER_SIZE (4) // Max length is 1024, so 4 characters
#define MQTT_SIGNING_BUFFER         (256)

#define MQTT_TLS_SECURITY_PROFILE_ID     (2)
#define MQTT_TLS_ECC_SECURITY_PROFILE_ID (1)

#define MQTT_URC_STATUS_CODE_INDEX (2)
#define STATUS_CODE_INVALID_VALUE  (3)
#define NUM_STATUS_CODES           (18)

#define HCESIGN_DIGEST_LENGTH (64)
#define HCESIGN_CTX_ID_LENGTH (5)

#define MQTT_TIMEOUT_MS (2000)

const char MQTT_RECEIVE[] PROGMEM = "AT+SQNSMQTTRCVMESSAGE=0,\"%s\"";
const char MQTT_RECEIVE_WITH_MSG_ID[] PROGMEM =
    "AT+SQNSMQTTRCVMESSAGE=0,\"%s\",%u";
const char MQTT_ON_MESSAGE_URC[] PROGMEM    = "SQNSMQTTONMESSAGE";
const char MQTT_ON_DISCONNECT_URC[] PROGMEM = "SQNSMQTTONDISCONNECT";
const char MQTT_DISCONNECT[] PROGMEM        = "AT+SQNSMQTTDISCONNECT=0";
const char HCESIGN[] PROGMEM                = "AT+SQNHCESIGN=%u,0,64,\"%s\"";

static const char STATUS_CODE_SUCCESS[] PROGMEM       = "Success";
static const char STATUS_CODE_NOMEM[] PROGMEM         = "No memory";
static const char STATUS_CODE_PROTOCOL[] PROGMEM      = "Protocol error";
static const char STATUS_CODE_INVAL[] PROGMEM         = "Invalid value";
static const char STATUS_CODE_NO_CONN[] PROGMEM       = "No connection";
static const char STATUS_CODE_CONN_REFUSED[] PROGMEM  = "Connection refused";
static const char STATUS_CODE_NOT_FOUND[] PROGMEM     = "Not found";
static const char STATUS_CODE_CONN_LOST[] PROGMEM     = "Connection lost";
static const char STATUS_CODE_TLS[] PROGMEM           = "TLS error";
static const char STATUS_CODE_PAYLOAD_SIZE[] PROGMEM  = "Payload size invalid";
static const char STATUS_CODE_NOT_SUPPORTED[] PROGMEM = "Not supported";
static const char STATUS_CODE_AUTH[] PROGMEM          = "Authentication error";
static const char STATUS_CODE_ACL_DENIED[] PROGMEM    = "ACL denied";
static const char STATUS_CODE_UNKNOWN[] PROGMEM       = "Unknown";
static const char STATUS_CODE_ERRNO[] PROGMEM         = "ERRNO";
static const char STATUS_CODE_EAI[] PROGMEM           = "EAI";
static const char STATUS_CODE_PROXY[] PROGMEM         = "Proxy error";
static const char STATUS_CODE_UNAVAILABLE[] PROGMEM   = "Unavailable";

/**
 * @brief Status codes from publish and subscribe MQTT commands.
 *
 * @note Both status code 2 and 3 are protocol invalid according to the AT
 * command reference.
 */
static PGM_P const STATUS_CODE_TABLE[NUM_STATUS_CODES] PROGMEM = {
    STATUS_CODE_SUCCESS,
    STATUS_CODE_NOMEM,
    STATUS_CODE_PROTOCOL,
    STATUS_CODE_INVAL,
    STATUS_CODE_NO_CONN,
    STATUS_CODE_CONN_REFUSED,
    STATUS_CODE_NOT_FOUND,
    STATUS_CODE_CONN_LOST,
    STATUS_CODE_TLS,
    STATUS_CODE_PAYLOAD_SIZE,
    STATUS_CODE_NOT_SUPPORTED,
    STATUS_CODE_AUTH,
    STATUS_CODE_ACL_DENIED,
    STATUS_CODE_UNKNOWN,
    STATUS_CODE_ERRNO,
    STATUS_CODE_EAI,
    STATUS_CODE_PROXY,
    STATUS_CODE_UNAVAILABLE};

MqttClientClass MqttClient = MqttClientClass::instance();

static volatile bool connected_to_broker = false;

/**
 * @brief Used when receiving messages to store the topic the messages was
 * received on.
 *
 * @note +3 since we need two extra characters for the parantheses and one extra
 * for null termination in the max case.
 */
static char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 3];

/**
 * @brief Used when waiting for URCs and for the receive callback. Functions as
 * a temporary buffer to store data.
 */
static char urc_buffer[URC_DATA_BUFFER_SIZE + 1];

static void (*disconnected_callback)(void)                = NULL;
static void (*receive_callback)(const char* topic,
                                const uint16_t message_length,
                                const int32_t message_id) = NULL;

static void internalDisconnectCallback(__attribute__((unused)) char* urc_data) {
    connected_to_broker = false;
    LedCtrl.off(Led::CON, true);

    if (disconnected_callback != NULL) {
        disconnected_callback();
    }
}

static void internalOnReceiveCallback(char* urc_data) {

    // The incoming urc_data is a buffer of maximum URC_DATA_BUFFER_SIZE (from
    // the SequansController)
    strncpy(urc_buffer, urc_data, URC_DATA_BUFFER_SIZE);

    // Safe guard ourselves
    urc_buffer[URC_DATA_BUFFER_SIZE] = '\0';

    const bool got_topic = SequansController.extractValueFromCommandResponse(
        urc_buffer,
        1,
        topic_buffer,
        sizeof(topic_buffer),
        0);

    if (!got_topic) {
        return;
    }

    // Remove parantheses at start and end
    char* topic              = topic_buffer + 1;
    topic[strlen(topic) - 1] = 0;

    char message_length_buffer[MQTT_MSG_LENGTH_BUFFER_SIZE + 1];

    const bool got_message_length =
        SequansController.extractValueFromCommandResponse(
            urc_buffer,
            2,
            message_length_buffer,
            sizeof(message_length_buffer),
            0);

    if (!got_message_length) {
        return;
    }

    char message_id_buffer[16];

    const bool got_message_id =
        SequansController.extractValueFromCommandResponse(
            urc_buffer,
            4,
            message_id_buffer,
            sizeof(message_id_buffer),
            0);

    // If there is no message ID, which is the case of MqttQoS is 0, then we
    // just specify -1.
    int32_t message_id = -1;

    if (got_message_id) {
        message_id = (int32_t)atoi(message_id_buffer);
    }

    if (receive_callback != NULL) {
        receive_callback(topic,
                         (uint16_t)atoi(message_length_buffer),
                         message_id);
    }
}

/**
 * @brief Takes in URC signing @p data, signs it and constructs a command
 * with the signature which is passed to the modem.
 *
 * @param data The data to sign.
 * @param command_buffer The constructed command with signature data.
 */
static bool generateSigningCommand(char* data, char* command_buffer) {

    // Grab the ctx id
    // +1 for null termination
    char ctx_id_buffer[HCESIGN_CTX_ID_LENGTH + 1];

    bool got_ctx_id = SequansController.extractValueFromCommandResponse(
        data,
        0,
        ctx_id_buffer,
        HCESIGN_CTX_ID_LENGTH + 1,
        (char)NULL);

    if (!got_ctx_id) {
        Log.error(F("Failed to generate signing command, no context ID!"));
        return false;
    }

    // Grab the digest, which will be 32 bytes, but appear as 64 hex
    // characters
    char digest[HCESIGN_DIGEST_LENGTH + 1];

    bool got_digest = SequansController.extractValueFromCommandResponse(
        data,
        3,
        digest,
        HCESIGN_DIGEST_LENGTH + 1,
        (char)NULL);

    if (!got_digest) {
        Log.error(F("Failed to generate signing command, no digest for signing "
                    "request!"));
        return false;
    }

    // Convert digest to 32 bytes
    uint8_t message_to_sign[HCESIGN_DIGEST_LENGTH / 2];
    char* position = digest;

    // Convert hex representation in string to numerical hex values
    for (uint8_t i = 0; i < sizeof(message_to_sign); i++) {
        sscanf(position, "%2hhx", &message_to_sign[i]);
        position += 2;
    }

    // Sign digest with ECC's primary private key
    const ATCA_STATUS result = atcab_sign(0, message_to_sign, (uint8_t*)digest);

    if (result != ATCA_SUCCESS) {
        Log.errorf(F("ECC signing failed, status code: %X\r\n"), result);
        return false;
    }

    // Now we need to convert the byte array into a hex string in
    // compact form
    const char hex_conversion[] = "0123456789abcdef";

    // +1 for NULL termination
    char signature[HCESIGN_DIGEST_LENGTH * 2 + 1] = "";

    // Prepare signature by converting to a hex string
    for (uint8_t i = 0; i < sizeof(digest) - 1; i++) {
        signature[i * 2]     = hex_conversion[(digest[i] >> 4) & 0x0F];
        signature[i * 2 + 1] = hex_conversion[digest[i] & 0x0F];
    }

    // NULL terminate
    signature[HCESIGN_DIGEST_LENGTH * 2] = 0;
    sprintf_P(command_buffer, HCESIGN, atoi(ctx_id_buffer), signature);

    return true;
}

bool MqttClientClass::beginAWS(const uint16_t keep_alive) {

    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf(F("Could not initialize ECC hardware, error code: %X\r\n"),
                   status);
        return false;
    }

    uint8_t thing_name[128];
    size_t thing_name_length = sizeof(thing_name);
    uint8_t endpoint[128];
    size_t endpointLen = sizeof(endpoint);

    status =
        ECC608.readProvisionItem(AWS_THINGNAME, thing_name, &thing_name_length);

    if (status != ATCA_SUCCESS) {

        if (status == ATCA_INVALID_ID) {
            Log.error(
                F("Could not find AWS thing name in the ECC. Please provision "
                  "the board for AWS using the instructions in the provision "
                  "sketch."));
            return false;
        }

        Log.errorf(
            F("Could not retrieve thing name from the ECC, error code: %X\r\n"),
            status);
        return false;
    }

    status = ECC608.readProvisionItem(AWS_ENDPOINT, endpoint, &endpointLen);

    if (status != ATCA_SUCCESS) {
        Log.errorf(
            F("Could not retrieve endpoint from the ECC, error code: %X\r\n"),
            status);
        return false;
    }

    Log.debugf(F("Connecting to AWS with endpoint: %s and thingname: %s\r\n"),
               endpoint,
               thing_name);

    return this->begin((char*)(thing_name),
                       (char*)(endpoint),
                       8883,
                       true,
                       keep_alive,
                       true,
                       "",
                       "");
}

bool MqttClientClass::beginAzure(const uint16_t keep_alive) {

    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf(F("Could not initialize ECC hardware, error code: %X\r\n"),
                   status);
        return false;
    }

    // Device ID is at maximum 20 characters (the serial number for the ECC is 9
    // digits converted to hexadecimal = 18 + 2 for "sn"). Add one for null
    // termination.
    char device_id[21]    = "";
    size_t device_id_size = sizeof(device_id);

    status = ECC608.readProvisionItem(AZURE_DEVICE_ID,
                                      (uint8_t*)device_id,
                                      &device_id_size);

    if (status != ATCA_SUCCESS) {

        if (status == ATCA_INVALID_ID) {
            Log.error(F("Could not find the Azure device ID in the ECC. Please "
                        "provision the board for Azure using the provision "
                        "example sketch."));
            return false;
        }

        Log.errorf(F("Failed to read device ID from ECC, error code: %X\r\n"),
                   status);
        return false;
    }

    char hostname[256]   = "";
    size_t hostname_size = sizeof(hostname);

    status = ECC608.readProvisionItem(AZURE_IOT_HUB_NAME,
                                      (uint8_t*)hostname,
                                      &hostname_size);

    if (status != ATCA_SUCCESS) {
        Log.errorf(F("Failed to read Azure IoT hub host name from ECC, error "
                     "code: %X\r\n"),
                   status);
        return false;
    }

    Log.debugf(F("Connecting to Azure with hostname: %s and device ID: %s\r\n"),
               hostname,
               device_id);

    // 24 comes from the format in the string below. Add +1 for NULL termination
    char username[sizeof(hostname) + 24 + sizeof(device_id) + 1] = "";

    snprintf_P(username,
               sizeof(username),
               PSTR("%s/%s/api-version=2018-06-30"),
               hostname,
               device_id);

    return this->begin(device_id,
                       hostname,
                       8883,
                       true,
                       keep_alive,
                       true,
                       username,
                       "");
}

bool MqttClientClass::begin(const char* client_id,
                            const char* host,
                            const uint16_t port,
                            const bool use_tls,
                            const uint16_t keep_alive,
                            const bool use_ecc,
                            const char* username,
                            const char* password,
                            const size_t timeout_ms,
                            const bool print_messages) {

    if (!Lte.isConnected()) {
        return false;
    }

    connected_to_broker = false;

    // Disconnect to terminate existing configuration
    //
    // We do this with writeString instead of writeCommand to not issue the
    // retries of the command if it fails.
    SequansController.writeString(FV(MQTT_DISCONNECT), true);

    // Force to read the result so that we don't go on with the next command
    // instantly. We just want to close the current connection if there are any.
    // If there aren't, this will return an error from the modem, but that is
    // fine as it just means that there aren't any connections active.
    SequansController.readResponse();

    // -- Configuration --

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls) {

        if (!SecurityProfile.profileExists(
                use_ecc ? MQTT_TLS_ECC_SECURITY_PROFILE_ID
                        : MQTT_TLS_SECURITY_PROFILE_ID)) {
            Log.error(F("Security profile not set up for MQTT TLS. "
                        "Run the 'provision' example Arduino sketch for more "
                        "instructions on how to set this up."));
            return false;
        }

        if (use_ecc) {

            const ATCA_STATUS status = ECC608.begin();

            if (status != ATCA_SUCCESS) {
                Log.errorf(
                    F("Could not initialize ECC hardware, error code: %X\r\n"),
                    status);
                return false;
            }
        }

        const ResponseResult configure_response =
            SequansController.writeCommand(
                F("AT+SQNSMQTTCFG=0,\"%s\",\"%s\",\"%s\",%u"),
                NULL,
                0,
                client_id,
                username,
                password,
                use_ecc ? MQTT_TLS_ECC_SECURITY_PROFILE_ID
                        : MQTT_TLS_SECURITY_PROFILE_ID);

        if (configure_response != ResponseResult::OK) {
            Log.errorf(
                F("Failed to configure MQTT. The TLS setup might be incorrect. "
                  "If you're using a custom broker with TLS, run the provision "
                  "example sketch in order to provision for a custom MQTT "
                  "broker "
                  "with TLS. Error code: %X\r\n"),
                static_cast<uint8_t>(configure_response));

            return false;
        }
    } else {

        const ResponseResult configure_response =
            SequansController.writeCommand(
                F("AT+SQNSMQTTCFG=0,\"%s\",\"%s\",\"%s\""),
                NULL,
                0,
                client_id,
                username,
                password);

        if (configure_response != ResponseResult::OK) {
            Log.errorf(F("Failed to configure MQTT, error code: %X\r\n"),
                       static_cast<uint8_t>(configure_response));
            return false;
        }
    }

    // -- Request connection --

    const ResponseResult connect_response = SequansController.writeCommand(
        F("AT+SQNSMQTTCONNECT=0,\"%s\",%u,%u"),
        NULL,
        0,
        host,
        port,
        keep_alive);

    if (connect_response != ResponseResult::OK) {
        Log.errorf(F("Failed to request connection to MQTT broker, error code: "
                     "%X\r\n"),
                   static_cast<uint8_t>(connect_response));
        return false;
    }

    if (print_messages) {
        Log.infof(F("Connecting to MQTT broker"));
    }

    // We are not allowed to capture local variables with the AVR compiler, so
    // we have to construct two lambdas.
    const auto toggle_led = [] { LedCtrl.toggle(Led::CON, true); };

    const auto toggle_led_with_printing = [] {
        LedCtrl.toggle(Led::CON, true);
        Log.rawf(F("."));
    };

    if (use_tls && use_ecc) {

        // Need to wait for a sign URC if we are using the ECC
        const bool got_sign_urc = SequansController.waitForURC(
            F("SQNHCESIGN"),
            urc_buffer,
            sizeof(urc_buffer),
            timeout_ms,
            print_messages ? toggle_led_with_printing : toggle_led,
            500);

        if (!got_sign_urc) {

            const char* error_message = PSTR(
                "Timed out whilst waiting for TLS signing. "
                "Please verify "
                "your certificate setup (run the provision Arduino "
                "sketch to set this up for a new broker).\r\n");

            if (print_messages) {
                Log.rawf(F(" %S\r\n"), error_message);
            } else {
                Log.errorf(F("%S\r\n"), error_message);
            }

            LedCtrl.off(Led::CON, true);
            return false;
        }

        char signing_request_buffer[MQTT_SIGNING_BUFFER + 1] = "";

        SequansController.startCriticalSection();
        const bool success = generateSigningCommand(urc_buffer,
                                                    signing_request_buffer);

        if (!success) {
            SequansController.stopCriticalSection();

            const char* error_message = PSTR(
                "Unable to handle signature request\r\n");

            if (print_messages) {
                Log.rawf(F(" %S\r\n"), error_message);
            } else {
                Log.errorf(F("%S\r\n"), error_message);
            }

            LedCtrl.off(Led::CON, true);
            return false;
        }

        SequansController.writeString(signing_request_buffer, true);
        SequansController.stopCriticalSection();
    }

    // Wait for connection response
    const bool got_connect_urc = SequansController.waitForURC(
        F("SQNSMQTTONCONNECT"),
        urc_buffer,
        sizeof(urc_buffer),
        timeout_ms,
        print_messages ? toggle_led_with_printing : toggle_led,
        500);

    if (!got_connect_urc) {
        const char* error_message = PSTR(
            "Timed out waiting for connection response.\r\n");

        if (print_messages) {
            Log.rawf(F(" %S\r\n"), error_message);
        } else {
            Log.errorf(F("%S\r\n"), error_message);
        }

        LedCtrl.off(Led::CON, true);
        return false;
    }

    // At most we can have two character ("-x"). We add an extra for null
    // termination
    char status_code_buffer[3] = "";

    if (!SequansController.extractValueFromCommandResponse(
            urc_buffer,
            MQTT_URC_STATUS_CODE_INDEX,
            status_code_buffer,
            sizeof(status_code_buffer),
            (char)NULL)) {

        const char* error_message = PSTR(
            "Failed to extract status code for connection.\r\n");

        if (print_messages) {
            Log.rawf(F(" %S\r\n"), error_message);
        } else {
            Log.errorf(F("%S\r\n"), error_message);
        }

        LedCtrl.off(Led::CON, true);
        return false;
    }

    // Status codes are reported as negative numbers, so we need to take the
    // absolute value. 0 is success.
    const uint8_t connection_response_code = abs(atoi(status_code_buffer));

    if (!connection_response_code) {

        if (print_messages) {
            Log.raw(F(" OK!"));
        }

        connected_to_broker = true;
        LedCtrl.on(Led::CON, true);

        SequansController.registerCallback(FV(MQTT_ON_DISCONNECT_URC),
                                           internalDisconnectCallback);
    } else {

        if (print_messages) {
            Log.rawf(F(" Unable to connect to broker: %S.\r\n"),
                     (PGM_P)pgm_read_word_far(
                         &(STATUS_CODE_TABLE[connection_response_code])));
        } else {
            Log.errorf(F("Unable to connect to broker: %S.\r\n"),
                       (PGM_P)pgm_read_word_far(
                           &(STATUS_CODE_TABLE[connection_response_code])));
        }

        connected_to_broker = false;
        LedCtrl.off(Led::CON, true);
    }

    return connected_to_broker;
}

bool MqttClientClass::end() {

    LedCtrl.off(Led::CON, true);

    SequansController.unregisterCallback(FV(MQTT_ON_MESSAGE_URC));
    SequansController.unregisterCallback(FV(MQTT_ON_DISCONNECT_URC));

    if (Lte.isConnected() && isConnected()) {
        SequansController.writeCommand(FV(MQTT_DISCONNECT));
        SequansController.clearReceiveBuffer();
    }

    connected_to_broker = false;

    if (disconnected_callback != NULL) {
        disconnected_callback();
    }

    return true;
}

void MqttClientClass::onConnectionStatusChange(
    __attribute__((unused)) void (*connected)(void),
    void (*disconnected)(void)) {

    if (disconnected != NULL) {
        disconnected_callback = disconnected;
    }
}

void MqttClientClass::onDisconnect(void (*disconnected)(void)) {
    if (disconnected != NULL) {
        disconnected_callback = disconnected;
    }
}

bool MqttClientClass::isConnected() { return connected_to_broker; }

bool MqttClientClass::publish(const char* topic,
                              const uint8_t* buffer,
                              const uint32_t buffer_size,
                              const MqttQoS quality_of_service,
                              const uint32_t timeout_ms) {

    if (!isConnected()) {
        Log.error(F("Attempted publish without being connected to a broker"));
        LedCtrl.off(Led::DATA, false);
        return false;
    }

    LedCtrl.on(Led::DATA, true);

    SequansController.writeString(F("AT+SQNSMQTTPUBLISH=0,\"%s\",%u,%lu"),
                                  true,
                                  topic,
                                  quality_of_service,
                                  buffer_size);

    // Wait for start character for delivering payload
    if (!SequansController.waitForByte('>', MQTT_TIMEOUT_MS)) {
        Log.warn(F("Timed out waiting to deliver MQTT payload."));

        LedCtrl.off(Led::DATA, true);
        return false;
    }

    Log.debugf(F("Publishing MQTT payload: %s\r\n"), buffer);

    SequansController.writeBytes(buffer, buffer_size);

    char urc[MQTT_PUBLISH_URC_LENGTH] = "";

    // At most we can have two character ("-x"). We add an extra for null
    // termination
    char status_code_buffer[3] = "";

    if (!SequansController.waitForURC(F("SQNSMQTTONPUBLISH"),
                                      urc,
                                      sizeof(urc),
                                      timeout_ms)) {
        Log.warn(F("Timed out waiting for publish confirmation. Consider "
                   "increasing timeout for publishing\r\n"));
        LedCtrl.off(Led::DATA, true);
        return false;
    }

    // The modem reports two URCs for publish, so we clear the other one
    SequansController.clearReceiveBuffer();

    if (!SequansController.extractValueFromCommandResponse(
            urc,
            MQTT_URC_STATUS_CODE_INDEX,
            status_code_buffer,
            sizeof(status_code_buffer),
            (char)NULL)) {

        Log.error(
            F("Failed to retrieve status code from publish notification"));
        LedCtrl.off(Led::DATA, true);
        return false;
    }

    // Status codes are reported as negative numbers, so we need to take the
    // absolute value
    const int8_t publish_status_code = abs(atoi(status_code_buffer));

    LedCtrl.off(Led::DATA, true);

    if (publish_status_code != 0) {
        Log.errorf(F("Error happened whilst publishing: %S.\r\n"),
                   (PGM_P)pgm_read_word_far(
                       &(STATUS_CODE_TABLE[publish_status_code])));
        return false;
    }

    return true;
}

bool MqttClientClass::publish(const char* topic,
                              const char* message,
                              const MqttQoS quality_of_service,
                              const uint32_t timeout_ms) {
    return publish(topic,
                   (uint8_t*)message,
                   strlen(message),
                   quality_of_service,
                   timeout_ms);
}

bool MqttClientClass::subscribe(const char* topic,
                                const MqttQoS quality_of_service) {

    if (!isConnected()) {
        Log.error(
            F("Attempted MQTT Subscribe without being connected to a broker"));
        return false;
    }

    const ResponseResult subscribe_result = SequansController.writeCommand(
        F("AT+SQNSMQTTSUBSCRIBE=0,\"%s\",%u"),
        NULL,
        0,
        topic,
        quality_of_service);

    if (subscribe_result != ResponseResult::OK) {
        Log.errorf(F("Failed to send subscribe command, error code: %x"),
                   static_cast<uint8_t>(subscribe_result));
        return false;
    }

    char urc[MQTT_SUBSCRIBE_URC_LENGTH] = "";

    // At most we can have two character ("-x"). We add an extra for null
    // termination
    char status_code_buffer[3] = "";

    if (!SequansController.waitForURC(F("SQNSMQTTONSUBSCRIBE"),
                                      urc,
                                      sizeof(urc))) {
        Log.error(F("Timed out waiting for subscribe confirmation\r\n"));
        return false;
    }

    if (!SequansController.extractValueFromCommandResponse(
            urc,
            MQTT_URC_STATUS_CODE_INDEX,
            status_code_buffer,
            sizeof(status_code_buffer),
            (char)NULL)) {

        Log.error(
            F("Failed to retrieve status code from subscribe notification"));
        return false;
    }

    // Status codes are reported as negative numbers, so we need to take the
    // absolute value
    const int8_t subscribe_status_code = abs(atoi(status_code_buffer));

    if (subscribe_status_code != 0) {

        Log.errorf(F("Error happened whilst subscribing: %S.\r\n"),
                   (PGM_P)pgm_read_word_far(
                       &(STATUS_CODE_TABLE[subscribe_status_code])));
        return false;
    }

    return true;
}

void MqttClientClass::onReceive(void (*callback)(const char* topic,
                                                 const uint16_t message_length,
                                                 const int32_t message_id)) {
    if (callback != NULL) {
        receive_callback = callback;
        SequansController.registerCallback(FV(MQTT_ON_MESSAGE_URC),
                                           internalOnReceiveCallback);
    }
}

bool MqttClientClass::readMessage(const char* topic,
                                  char* buffer,
                                  const uint16_t buffer_size,
                                  const int32_t message_id) {
    if (buffer_size > MQTT_MSG_MAX_BUFFER_SIZE) {

        Log.errorf(F("MQTT message is longer than the max size of %d\r\n"),
                   MQTT_MSG_MAX_BUFFER_SIZE);
        return false;
    }

    // We don't use writeCommand here as the AT receive command for MQTT
    // will return a carraige return and a line feed before the content, so
    // we write the bytes and manually clear these character before the
    // payload
    SequansController.clearReceiveBuffer();

    // We determine all message IDs lower than 0 as just no message ID passed
    if (message_id < 0) {
        SequansController.writeString(FV(MQTT_RECEIVE), true, topic);

    } else {
        SequansController.writeString(FV(MQTT_RECEIVE_WITH_MSG_ID),
                                      true,
                                      topic,
                                      (unsigned int)message_id);
    }

    // First two bytes are \r\n for the MQTT message response, so we flush those
    if (!SequansController.waitForByte('\r', 100)) {
        return false;
    }
    if (!SequansController.waitForByte('\n', 100)) {
        return false;
    }

    const ResponseResult receive_response =
        SequansController.readResponse(buffer, buffer_size);

    return (receive_response == ResponseResult::OK);
}

String MqttClientClass::readMessage(const char* topic, const uint16_t size) {
    Log.debugf(F("Reading message on topic %s\r\n"), topic);

    // Add bytes for termination of AT command when reading
    char buffer[size + 16];

    if (!readMessage(topic, buffer, sizeof(buffer))) {
        return "";
    }

    return buffer;
}

void MqttClientClass::clearMessages(const char* topic,
                                    const uint16_t num_messages) {

    for (uint16_t i = 0; i < num_messages; i++) {
        SequansController.writeCommand(FV(MQTT_RECEIVE), NULL, 0, topic);
    }
}
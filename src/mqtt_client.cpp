#include "mqtt_client.h"
#include "ecc608.h"
#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "security_profile.h"
#include "sequans_controller.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CONFIGURE           "AT+SQNSMQTTCFG=0,\"%s\",\"%s\",\"%s\""
#define MQTT_CONFIGURE_TLS       "AT+SQNSMQTTCFG=0,\"%s\",\"%s\",\"%s\",%u"
#define MQTT_CONNECT             "AT+SQNSMQTTCONNECT=0,\"%s\",%u,%u"
#define MQTT_DISCONNECT          "AT+SQNSMQTTDISCONNECT=0"
#define MQTT_PUBLISH             "AT+SQNSMQTTPUBLISH=0,\"%s\",%u,%lu"
#define MQTT_SUSBCRIBE           "AT+SQNSMQTTSUBSCRIBE=0,\"%s\",%u"
#define MQTT_RECEIVE             "AT+SQNSMQTTRCVMESSAGE=0,\"%s\""
#define MQTT_RECEIVE_WITH_MSG_ID "AT+SQNSMQTTRCVMESSAGE=0,\"%s\",%u"
#define MQTT_ON_MESSAGE_URC      "SQNSMQTTONMESSAGE"
#define MQTT_ON_CONNECT_URC      "SQNSMQTTONCONNECT"
#define MQTT_ON_DISCONNECT_URC   "SQNSMQTTONDISCONNECT"
#define MQTT_ON_PUBLISH_URC      "SQNSMQTTONPUBLISH"
#define MQTT_ON_SUBSCRIBE_URC    "SQNSMQTTONSUBSCRIBE"

// Command without any data in it (with quotation marks): 25 bytes
// Termination: 1 byte
// Total: 26 bytes
#define MQTT_CONFIGURE_LENGTH 26

// Command with security profile ID (with quotation marks): 27 bytes
// Termination: 1 byte
// Total: 28 bytes
#define MQTT_CONFIGURE_TLS_LENGTH 28

// Command without any data in it (with quotation marks): 25 bytes
// Hostname: 127 bytes
// Port: 5 bytes
// Termination: 1 byte
// Total: 157 bytes
#define MQTT_CONNECT_LENGTH_PRE_KEEP_ALIVE 158

#define MQTT_DISCONNECT_LENGTH 24

// Command without any data in it (with quotation marks): 25 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// QoS: 1 byte
// Termination: 1 byte
// Total: 155 bytes
//
// Note that we don't add the length of the data here as it is not fixed
#define MQTT_PUBLISH_LENGTH_PRE_DATA 155

// Command without any data in it (with quotation marks): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// QoS: 1 byte
// Termination: 1 byte
// Total: 156 bytes
#define MQTT_SUBSCRIBE_LENGTH 157

// Command without any data in it (with quotation marks): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 155 bytes
#define MQTT_RECEIVE_LENGTH 155

// Command without any data in it (with quotation marks): 27 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// Message ID: N bytes (runtime determined)
// Termination: 1 byte
// Total: 156 bytes
#define MQTT_RECEIVE_WITH_MSG_ID_LENGTH 156

#define MQTT_PUBLISH_URC_LENGTH            32
#define MQTT_SUBSCRIBE_URC_LENGTH          164
#define MQTT_PUB_SUB_URC_STATUS_CODE_INDEX 2

// This is the index in characters, including delimiter in the connection URC.
#define MQTT_CONNECTION_RC_INDEX  2
#define MQTT_CONNECTION_RC_LENGTH 3

#define MQTT_CONNECTION_SUCCESS_RC '0'

// This is a limitation from the sequans module
#define MQTT_MAX_BUFFER_SIZE 1024

// Max length is 1024, so 4 characters
#define MQTT_MSG_LENGTH_BUFFER_SIZE 4

// Identifiers passed to the configure command
#define MQTT_TLS_SECURITY_PROFILE_ID     2
#define MQTT_TLS_ECC_SECURITY_PROFILE_ID 1

#define MQTT_SIGNING_BUFFER 256

#define HCESIGN "AT+SQNHCESIGN=%u,0,64,\"%s\""

// Command without any data in it (with parantheses): 22 bytes
// ctxId: 5 bytes (16 bits, thus 5 characters max)
// Signature: 128 bytes
// Termination: 1 byte
// Total: 156 bytes
#define HCESIGN_LENGTH 156

#define HCESIGN_URC "SQNHCESIGN"

#define HCESIGN_REQUEST_LENGTH 128
#define HCESIGN_DIGEST_LENGTH  64
#define HCESIGN_CTX_ID_LENGTH  5

#define SIGN_TIMEOUT_MS       20000
#define MQTT_TIMEOUT_MS       2000
#define DISCONNECT_TIMEOUT_MS 20000

#define NUM_STATUS_CODES    18
#define STATUS_CODE_PENDING (-1)

// Singleton instance
MqttClientClass MqttClient = MqttClientClass::instance();

static volatile bool connected_to_broker   = false;
static void (*connected_callback)(void)    = NULL;
static void (*disconnected_callback)(void) = NULL;

static char receive_urc_buffer[URC_DATA_BUFFER_SIZE + 1];

// +3 since we need two extra characters for the parantheses and one
// extra for null termination in the max case
static char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 3];

static void (*receive_callback)(const char* topic,
                                const uint16_t message_length,
                                const int32_t message_id) = NULL;

/**
 * @brief Status codes from publish and subscribe MQTT commands.
 *
 * @note Both status code 2 and 3 are protocol invalid according to the AT
 * command reference.
 */
static char status_code_table[NUM_STATUS_CODES][24] = {"Success",
                                                       "No memory",
                                                       "Protocol invalid",
                                                       "Protocol invalid",
                                                       "No connection",
                                                       "Connection refused",
                                                       "Not found",
                                                       "Connection lost",
                                                       "TLS error",
                                                       "Payload size invalid",
                                                       "Not supported",
                                                       "Authentication error",
                                                       "ACL denied",
                                                       "Unknown",
                                                       "ERRNO",
                                                       "EAI",
                                                       "Proxy error",
                                                       "Pending"};

/**
 * @brief Called on MQTT broker connection URC. Will check the URC to see if the
 * connection was successful.
 */
static void internalConnectedCallback(char* urc_data) {
    if (urc_data[MQTT_CONNECTION_RC_INDEX] == MQTT_CONNECTION_SUCCESS_RC) {

        connected_to_broker = true;
        LedCtrl.on(Led::CON, true);

        if (connected_callback != NULL) {
            connected_callback();
        }
    } else {
        connected_to_broker = false;
        LedCtrl.off(Led::CON, true);

        if (disconnected_callback != NULL) {
            disconnected_callback();
        }
    }
}

static void internalDisconnectCallback(char* urc_data) {
    connected_to_broker = false;
    LedCtrl.off(Led::CON, true);

    if (disconnected_callback != NULL) {
        disconnected_callback();
    }
}

static void internalOnReceiveCallback(char* urc_data) {

    strncpy(receive_urc_buffer, urc_data, URC_DATA_BUFFER_SIZE);

    bool got_topic = SequansController.extractValueFromCommandResponse(
        receive_urc_buffer,
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

    bool got_message_length = SequansController.extractValueFromCommandResponse(
        receive_urc_buffer,
        2,
        message_length_buffer,
        sizeof(message_length_buffer),
        0);

    if (!got_message_length) {
        return;
    }

    char message_id_buffer[16];

    bool got_message_id = SequansController.extractValueFromCommandResponse(
        receive_urc_buffer,
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
        Log.error("No context ID!");
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
        Log.error("No digest for signing request!");
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
    ATCA_STATUS result = atcab_sign(0, message_to_sign, (uint8_t*)digest);

    if (result != ATCA_SUCCESS) {
        Log.errorf("ECC signing failed, status code: %x\r\n", result);
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
    sprintf(command_buffer, HCESIGN, atoi(ctx_id_buffer), signature);

    return true;
}

bool MqttClientClass::beginAWS() {

    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf("Could not initialize ECC hardware, error code: %X\r\n",
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
                "Could not find AWS thing name in the ECC. Please provision "
                "the board for AWS using the iotprovision tool.");
            return false;
        }

        Log.errorf(
            "Could not retrieve thing name from the ECC, error code: %X\r\n",
            status);
        return false;
    }

    status = ECC608.readProvisionItem(AWS_ENDPOINT, endpoint, &endpointLen);

    if (status != ATCA_SUCCESS) {
        Log.errorf(
            "Could not retrieve endpoint from the ECC, error code: %X\r\n",
            status);
        return false;
    }

    Log.debugf("Connecting to AWS with endpoint: %s and thingname: %s\r\n",
               endpoint,
               thing_name);

    return this->begin((char*)(thing_name),
                       (char*)(endpoint),
                       8883,
                       true,
                       60,
                       true,
                       "",
                       "");
}

bool MqttClientClass::beginAzure() {

    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf("Could not initialize ECC hardware, error code: %X\r\n",
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
            Log.error("Could not find the Azure device ID in the ECC. Please "
                      "provision the board for Azure using the provision "
                      "example sketch.");
            return false;
        }

        Log.errorf("Failed to read device ID from ECC, error code: %X\r\n",
                   status);
        return false;
    }

    char hostname[256]   = "";
    size_t hostname_size = sizeof(hostname);

    status = ECC608.readProvisionItem(AZURE_IOT_HUB_NAME,
                                      (uint8_t*)hostname,
                                      &hostname_size);

    if (status != ATCA_SUCCESS) {
        Log.errorf("Failed to read Azure IoT hub host name from ECC, error "
                   "code: %X\r\n",
                   status);
        return false;
    }

    Log.debugf("Connecting to Azure with hostname: %s and device ID: %s\r\n",
               hostname,
               device_id);

    // 24 comes from the format in the string below. Add +1 for NULL termination
    char username[sizeof(hostname) + 24 + sizeof(device_id) + 1] = "";

    snprintf(username,
             sizeof(username),
             "%s/%s/api-version=2018-06-30",
             hostname,
             device_id);

    return this->begin(device_id, hostname, 8883, true, 60, true, username, "");
}

bool MqttClientClass::begin(const char* client_id,
                            const char* host,
                            const uint16_t port,
                            const bool use_tls,
                            const size_t keep_alive,
                            const bool use_ecc,
                            const char* username,
                            const char* password) {

    if (!Lte.isConnected()) {
        return false;
    }

    // Disconnect to terminate existing configuration
    SequansController.writeBytes((uint8_t*)MQTT_DISCONNECT,
                                 strlen(MQTT_DISCONNECT),
                                 true);

    // Force to read the result so that we don't go on with the next command
    // instantly. We just want to close the current connection if there are any.
    // If there aren't, this will return an error from the modem, but that is
    // fine as it just means that there aren't any connections active.
    //
    // We do this with writeBytes instead of writeCommand to not issue the
    // retries of the command if it fails.
    SequansController.readResponse();

    SequansController.clearReceiveBuffer();

    // -- Configuration --

    const size_t client_id_length = strlen(client_id);
    const size_t username_length  = strlen(username);
    const size_t password_length  = strlen(password);

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls) {

        char command[MQTT_CONFIGURE_TLS_LENGTH + client_id_length +
                     username_length + password_length] = "";

        if (use_ecc) {

            if (!SecurityProfile.profileExists(
                    MQTT_TLS_ECC_SECURITY_PROFILE_ID)) {
                Log.error("Security profile not set up for MQTT TLS with ECC. "
                          "Run the 'provision' example Arduino sketch to set "
                          "this up for a custom broker or use the iotprovision "
                          "tool to set this up for AWS.");
                return false;
            }

            uint8_t status = ECC608.begin();
            if (status != ATCA_SUCCESS) {
                Log.error("Could not initialize ECC hardware");
                return false;
            }

            sprintf(command,
                    MQTT_CONFIGURE_TLS,
                    client_id,
                    username,
                    password,
                    MQTT_TLS_ECC_SECURITY_PROFILE_ID);

        } else {
            if (!SecurityProfile.profileExists(MQTT_TLS_SECURITY_PROFILE_ID)) {
                Log.error("Security profile not set up for MQTT TLS. Run the "
                          "'provision' example Arduino sketch to set this up.");
                return false;
            }

            sprintf(command,
                    MQTT_CONFIGURE_TLS,
                    client_id,
                    username,
                    password,
                    MQTT_TLS_SECURITY_PROFILE_ID);
        }

        if (SequansController.writeCommand(command) != ResponseResult::OK) {
            Log.error(
                "Failed to configure MQTT. The TLS setup might be incorrect. "
                "If you're using a custom broker with TLS, run the provision "
                "example sketch in order to provision for a custom MQTT broker "
                "with TLS.");
            return false;
        }
    } else {
        char command[MQTT_CONFIGURE_LENGTH + client_id_length +
                     username_length + password_length] = "";
        sprintf(command, MQTT_CONFIGURE, client_id, username, password);

        if (SequansController.writeCommand(command) != ResponseResult::OK) {
            Log.error("Failed to configure MQTT");
            return false;
        }
    }

    SequansController.registerCallback(MQTT_ON_CONNECT_URC,
                                       internalConnectedCallback);
    SequansController.registerCallback(MQTT_ON_DISCONNECT_URC,
                                       internalDisconnectCallback);

    // -- Request connection --
    size_t keep_alive_length = floor(log10(keep_alive)) + 1;
    char command[MQTT_CONNECT_LENGTH_PRE_KEEP_ALIVE + keep_alive_length] = "";

    sprintf(command, MQTT_CONNECT, host, port, keep_alive);

    if (SequansController.writeCommand(command) != ResponseResult::OK) {
        Log.error("Failed to request connection to MQTT broker\r\n");
        return false;
    }

    if (use_tls && use_ecc) {

        char urc[URC_DATA_BUFFER_SIZE] = "";
        if (!SequansController.waitForURC(HCESIGN_URC, urc, sizeof(urc))) {
            Log.error("Timed out whilst waiting for TLS signing. Please verify "
                      "your certificate setup (run the provision Arduino "
                      "sketch to set this up for a new broker).\r\n");
            return false;
        }

        char signing_request_buffer[MQTT_SIGNING_BUFFER + 1] = "";

        SequansController.startCriticalSection();
        bool success = generateSigningCommand(urc, signing_request_buffer);

        if (success != true) {
            Log.error("Unable to handle signature request\r\n");
            return false;
        }

        SequansController.writeBytes((uint8_t*)signing_request_buffer,
                                     strlen(signing_request_buffer),
                                     true);

        SequansController.stopCriticalSection();
    }

    return true;
}

bool MqttClientClass::end(void) {

    LedCtrl.off(Led::CON, true);

    SequansController.unregisterCallback(MQTT_ON_MESSAGE_URC);
    SequansController.unregisterCallback(MQTT_ON_CONNECT_URC);
    SequansController.unregisterCallback(MQTT_ON_DISCONNECT_URC);

    if (Lte.isConnected() && isConnected()) {

        SequansController.writeCommand(MQTT_DISCONNECT);
        SequansController.clearReceiveBuffer();
    }

    connected_to_broker = false;

    if (disconnected_callback != NULL) {
        disconnected_callback();
    }

    return true;
}

void MqttClientClass::onConnectionStatusChange(void (*connected)(void),
                                               void (*disconnected)(void)) {
    if (connected != NULL) {
        connected_callback = connected;
    }

    if (disconnected != NULL) {
        disconnected_callback = disconnected;
    }
}

bool MqttClientClass::isConnected(void) { return connected_to_broker; }

bool MqttClientClass::publish(const char* topic,
                              const uint8_t* buffer,
                              const uint32_t buffer_size,
                              const MqttQoS quality_of_service,
                              const uint32_t timeout_ms) {

    if (!isConnected()) {
        Log.error("Attempted publish without being connected to a broker");
        LedCtrl.off(Led::DATA, false);
        return false;
    }

    LedCtrl.on(Led::DATA, true);

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;
    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command,
            MQTT_PUBLISH,
            topic,
            quality_of_service,
            (unsigned long)buffer_size);

    SequansController.writeBytes((uint8_t*)command, strlen(command), true);

    // Wait for start character for delivering payload
    if (!SequansController.waitForByte('>', MQTT_TIMEOUT_MS)) {
        Log.warn("Timed out waiting to deliver MQTT payload.");

        LedCtrl.off(Led::DATA, true);
        return false;
    }

    Log.debugf("Publishing MQTT payload: %s\r\n", buffer);

    SequansController.writeBytes(buffer, buffer_size);

    char urc[MQTT_PUBLISH_URC_LENGTH] = "";

    // At most we can have two character ("-1"). We add an extra for null
    // termination
    char status_code_buffer[3] = "";

    if (!SequansController.waitForURC(MQTT_ON_PUBLISH_URC,
                                      urc,
                                      sizeof(urc),
                                      timeout_ms)) {
        Log.warn("Timed out waiting for publish confirmation. Consider "
                 "increasing timeout for publishing\r\n");
        return false;
    }

    // The modem reports two URCs for publish, so we clear the other one
    SequansController.clearReceiveBuffer();

    if (!SequansController.extractValueFromCommandResponse(
            urc,
            MQTT_PUB_SUB_URC_STATUS_CODE_INDEX,
            status_code_buffer,
            sizeof(status_code_buffer),
            (char)NULL)) {

        Log.error("Failed to retrieve status code from publish notification");
        return false;
    }

    int8_t publish_status_code = atoi(status_code_buffer);

    // One of the status codes is -1, so in order to not overflow the status
    // code table, we swap the place with the last one
    if (publish_status_code == STATUS_CODE_PENDING) {
        publish_status_code = NUM_STATUS_CODES - 1;
    }

    LedCtrl.off(Led::DATA, true);

    if (publish_status_code != 0) {
        Log.errorf("Error happened while publishing: %s\r\n",
                   status_code_table[publish_status_code]);
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
            "Attempted MQTT Subscribe without being connected to a broker");
        return false;
    }

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic, quality_of_service);

    if (SequansController.writeCommand(command) != ResponseResult::OK) {
        Log.error("Failed to send subscribe command");
        return false;
    }

    char urc[MQTT_SUBSCRIBE_URC_LENGTH] = "";

    // At most we can have two character ("-1"). We add an extra for null
    // termination
    char status_code_buffer[3] = "";

    if (!SequansController.waitForURC(MQTT_ON_SUBSCRIBE_URC,
                                      urc,
                                      sizeof(urc))) {
        Log.warn("Timed out waiting for subscribe confirmation\r\n");
        return false;
    }

    if (!SequansController.extractValueFromCommandResponse(
            urc,
            MQTT_PUB_SUB_URC_STATUS_CODE_INDEX,
            status_code_buffer,
            sizeof(status_code_buffer),
            (char)NULL)) {

        Log.error("Failed to retrieve status code from subscribe notification");
        return false;
    }

    int8_t subscribe_status_code = atoi(status_code_buffer);

    // One of the status codes is -1, so in order to not overflow the status
    // code table, we swap the place with the last one
    if (subscribe_status_code == STATUS_CODE_PENDING) {
        subscribe_status_code = NUM_STATUS_CODES - 1;
    }

    if (subscribe_status_code != 0) {
        Log.errorf("Error happened while subscribing: %s\r\n",
                   status_code_table[subscribe_status_code]);
        return false;
    }

    return true;
}

void MqttClientClass::onReceive(void (*callback)(const char* topic,
                                                 const uint16_t message_length,
                                                 const int32_t message_id)) {
    if (callback != NULL) {
        receive_callback = callback;
        SequansController.registerCallback(MQTT_ON_MESSAGE_URC,
                                           internalOnReceiveCallback);
    }
}

bool MqttClientClass::readMessage(const char* topic,
                                  char* buffer,
                                  const uint16_t buffer_size,
                                  const int32_t message_id) {
    if (buffer_size > MQTT_MAX_BUFFER_SIZE) {

        Log.errorf("MQTT message is longer than the max size of %d\r\n",
                   MQTT_MAX_BUFFER_SIZE);
        return false;
    }

    // We don't use writeCommand here as the AT receive command for MQTT
    // will return a carraige return and a line feed before the content, so
    // we write the bytes and manually clear these character before the
    // payload

    SequansController.clearReceiveBuffer();

    // We determine all message IDs lower than 0 as just no message ID passed
    if (message_id < 0) {
        char command[MQTT_RECEIVE_LENGTH] = "";
        sprintf(command, MQTT_RECEIVE, topic);

        SequansController.writeBytes((uint8_t*)&command[0],
                                     strlen(command),
                                     true);

    } else {
        const uint32_t digits_in_msg_id = trunc(log10(message_id)) + 1;
        char command[MQTT_RECEIVE_WITH_MSG_ID_LENGTH + digits_in_msg_id] = "";
        sprintf(command,
                MQTT_RECEIVE_WITH_MSG_ID,
                topic,
                (unsigned int)message_id);

        SequansController.writeBytes((uint8_t*)&command[0],
                                     strlen(command),
                                     true);
    }

    // First two bytes are \r\n for the MQTT message response, so we flush those
    if (!SequansController.waitForByte('\r', 100)) {
        return false;
    }
    if (!SequansController.waitForByte('\n', 100)) {
        return false;
    }

    // Then we can read the response into the buffer
    const ResponseResult response = SequansController.readResponse(buffer,
                                                                   buffer_size);

    return (response == ResponseResult::OK);
}

String MqttClientClass::readMessage(const char* topic, const uint16_t size) {
    Log.debugf("Reading message on topic %s\r\n", topic);
    // Add bytes for termination of AT command when reading
    char buffer[size + 16];
    if (!readMessage(topic, buffer, sizeof(buffer))) {
        return "";
    }

    return buffer;
}

void MqttClientClass::clearMessages(const char* topic,
                                    const uint16_t num_messages) {

    char command[MQTT_RECEIVE_LENGTH] = "";
    sprintf(command, MQTT_RECEIVE, topic);

    for (uint16_t i = 0; i < num_messages; i++) {
        SequansController.writeCommand(command);
    }
}
#include "ecc608.h"
#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "mqtt_client.h"
#include "sequans_controller.h"

#include <cryptoauthlib.h>
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

static bool connected_to_broker = false;
static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;

static volatile bool signing_request_flag = false;
static char signing_request_buffer[MQTT_SIGNING_BUFFER + 1];
static char signing_urc[URC_DATA_BUFFER_SIZE + 1];

static char receive_urc_buffer[URC_DATA_BUFFER_SIZE + 1];

// +3 since we need two extra characters for the parantheses and one
// extra for null termination in the max case
static char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 3];

static void (*receive_callback)(const char *topic,
                                const uint16_t message_length,
                                const int32_t message_id) = NULL;

static bool using_ecc = false;

static volatile bool got_publish_urc = false;
static volatile int8_t publish_status_code = 0;

static volatile bool got_subscribe_urc = false;
static volatile int8_t subscribe_status_code = 0;

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
static void internalConnectedCallback(char *urc_data) {
    if (urc_data[MQTT_CONNECTION_RC_INDEX] == MQTT_CONNECTION_SUCCESS_RC) {

        connected_to_broker = true;
        LedCtrl.on(Led::CON, true);
        if (connected_callback != NULL) {
            connected_callback();
        }
    } else {
        LedCtrl.off(Led::CON, true);
        connected_to_broker = false;
        if (disconnected_callback != NULL) {
            disconnected_callback();
        }
    }
}

static void internalDisconnectCallback(char *urc_data) {
    connected_to_broker = false;

    if (disconnected_callback != NULL) {
        disconnected_callback();
    }
}

static void internalHandleSigningRequest(char *urc_data) {
    strncpy(signing_urc, urc_data, URC_DATA_BUFFER_SIZE);
    signing_request_flag = true;
}

static void internalOnReceiveCallback(char *urc_data) {

    strncpy(receive_urc_buffer, urc_data, URC_DATA_BUFFER_SIZE);

    bool got_topic = SequansController.extractValueFromCommandResponse(
        receive_urc_buffer, 1, topic_buffer, sizeof(topic_buffer), 0);

    if (!got_topic) {
        return;
    }

    // Remove parantheses at start and end
    char *topic = topic_buffer + 1;
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
        receive_urc_buffer, 4, message_id_buffer, sizeof(message_id_buffer), 0);

    // If there is no message ID, which is the case of MqttQoS is 0, then we
    // just specify -1.
    int32_t message_id = -1;

    if (got_message_id) {
        message_id = (int32_t)atoi(message_id_buffer);
    }

    if (receive_callback != NULL) {
        receive_callback(
            topic, (uint16_t)atoi(message_length_buffer), message_id);
    }
}

/**
 * @brief Called on SQNSMQTTONPUBLISH URC.
 */
static void internalPublishCallback(char *urc_data) {

    uint8_t num_deliminators = 0;

    // The URC will have the payload on the form x,y,status_code where we are
    // interrested in the status code, so jump towards the last value
    while (num_deliminators < 2) {
        if (*urc_data == ',') {
            num_deliminators++;
        }

        urc_data++;
    }

    publish_status_code = atoi(urc_data);

    // One of the status codes is -1, so in order to not overflow the status
    // code table, we swap the place with the last one
    if (publish_status_code == STATUS_CODE_PENDING) {
        publish_status_code = NUM_STATUS_CODES - 1;
    }

    got_publish_urc = true;
}

/**
 * @brief Called on SQNSMQTTONSUBSCRIBE URC.
 */
static void internalSubscribeCallback(char *urc_data) {
    uint8_t num_deliminators = 0;

    // The URC will have the payload on the form x,y,status_code where we are
    // interrested in the status code, so jump towards the last value
    while (num_deliminators < 2) {
        if (*urc_data == ',') {
            num_deliminators++;
        }

        urc_data++;
    }

    subscribe_status_code = atoi(urc_data);

    // One of the status codes is -1, so in order to not overflow the status
    // code table, we swap the place with the last one
    if (subscribe_status_code == STATUS_CODE_PENDING) {
        subscribe_status_code = NUM_STATUS_CODES - 1;
    }

    got_subscribe_urc = true;
}

/**
 * @brief Takes in URC signing @p data, signs it and constructs a command
 * with the signature which is passed to the modem.
 *
 * @param data The data to sign.
 * @param command_buffer The constructed command with signature data.
 */
static bool generateSigningCommand(char *data, char *command_buffer) {

    char sign_request[HCESIGN_REQUEST_LENGTH] = "SQNHCESIGN:";
    strncpy(sign_request + 11, data, HCESIGN_REQUEST_LENGTH - 11);

    // Grab the ctx id
    // +1 for null termination
    char ctx_id_buffer[HCESIGN_CTX_ID_LENGTH + 1];

    bool got_ctx_id = SequansController.extractValueFromCommandResponse(
        sign_request, 0, ctx_id_buffer, HCESIGN_CTX_ID_LENGTH + 1);

    if (!got_ctx_id) {
        Log.error("No context ID!");
        return false;
    }

    // Grab the digest, which will be 32 bytes, but appear as 64 hex
    // characters
    char digest[HCESIGN_DIGEST_LENGTH + 1];

    bool got_digest = SequansController.extractValueFromCommandResponse(
        sign_request, 3, digest, HCESIGN_DIGEST_LENGTH + 1);

    if (!got_digest) {
        Log.error("No digest for signing request!");
        return false;
    }

    // Convert digest to 32 bytes
    uint8_t message_to_sign[HCESIGN_DIGEST_LENGTH / 2];
    char *position = digest;

    for (uint8_t i = 0; i < sizeof(message_to_sign); i++) {
        sscanf(position, "%2hhx", &message_to_sign[i]);
        position += 2;
    }

    // Sign digest with ECC's primary private key
    ATCA_STATUS result = atcab_sign(0, message_to_sign, (uint8_t *)digest);

    if (result != ATCA_SUCCESS) {
        Log.error("ECC signing failed");
        return false;
    }

    // Now we need to convert the byte array into a hex string in
    // compact form
    const char hex_conversion[] = "0123456789abcdef";

    // +1 for NULL termination
    char signature[HCESIGN_DIGEST_LENGTH * 2 + 1] = "";

    // Prepare signature by converting to a hex string
    for (uint8_t i = 0; i < sizeof(digest) - 1; i++) {
        signature[i * 2] = hex_conversion[(digest[i] >> 4) & 0x0F];
        signature[i * 2 + 1] = hex_conversion[digest[i] & 0x0F];
    }

    // NULL terminate
    signature[HCESIGN_DIGEST_LENGTH * 2] = 0;
    sprintf(command_buffer, HCESIGN, atoi(ctx_id_buffer), signature);

    return true;
}

/**
 * @brief Sends the signed message from the ECC back to the Sequans modem
 */
static bool signIncomingRequests(void) {
    if (signing_request_flag) {

        SequansController.startCriticalSection();
        bool success =
            generateSigningCommand(signing_urc, signing_request_buffer);

        if (success != true) {
            Log.error("Unable to handle signature request\r\n");
            return false;
        }

        bool retVal = SequansController.writeCommand(signing_request_buffer);
        signing_request_flag = false;
        SequansController.stopCriticalSection();

        return retVal;
    }

    return false;
}

bool MqttClientClass::beginAWS() {
    // Get the endoint and thing name
    // -- Initialize the ECC
    uint8_t err = ECC608.begin();
    if (err != ATCA_SUCCESS) {
        Log.error("Could not initialize ECC hardware");
        return false;
    }

    // -- Allocate the buffers
    uint8_t thingName[128];
    uint8_t thingNameLen = sizeof(thingName);
    uint8_t endpoint[128];
    uint8_t endpointLen = sizeof(endpoint);

    // -- Get the thingname
    err = ECC608.getThingName(thingName, &thingNameLen);
    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve thing name from the ECC");
        return false;
    }

    // -- Get the endpoint
    err = ECC608.getEndpoint(endpoint, &endpointLen);

    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve endpoint from the ECC");
        return false;
    }

    Log.debugf("Connecting to AWS with endpoint = %s and thingname = %s\r\n",
               endpoint,
               thingName);

    using_ecc = true;

    return this->begin(
        (char *)(thingName), (char *)(endpoint), 8883, true, 60, true, "", "");
}

bool MqttClientClass::begin(const char *client_id,
                            const char *host,
                            const uint16_t port,
                            const bool use_tls,
                            const size_t keep_alive,
                            const bool use_ecc,
                            const char *username,
                            const char *password) {

    // -- Configuration --

    const size_t client_id_length = strlen(client_id);
    const size_t username_length = strlen(username);
    const size_t password_length = strlen(password);

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls) {

        char command[MQTT_CONFIGURE_TLS_LENGTH + client_id_length +
                     username_length + password_length] = "";

        if (use_ecc) {
            sprintf(command,
                    MQTT_CONFIGURE_TLS,
                    client_id,
                    username,
                    password,
                    MQTT_TLS_ECC_SECURITY_PROFILE_ID);
        } else {
            sprintf(command,
                    MQTT_CONFIGURE_TLS,
                    client_id,
                    username,
                    password,
                    MQTT_TLS_SECURITY_PROFILE_ID);
        }

        SequansController.writeCommand(command);
    } else {
        char command[MQTT_CONFIGURE_LENGTH + client_id_length +
                     username_length + password_length] = "";
        sprintf(command, MQTT_CONFIGURE, client_id, username, password);
        SequansController.writeCommand(command);
    }

    if (SequansController.readResponse() != ResponseResult::OK) {
        Log.error("Failed to configure MQTT");
        return false;
    }

    SequansController.registerCallback(MQTT_ON_CONNECT_URC,
                                       internalConnectedCallback);
    SequansController.registerCallback(MQTT_ON_DISCONNECT_URC,
                                       internalDisconnectCallback);
    SequansController.registerCallback(HCESIGN_URC,
                                       internalHandleSigningRequest);

    // -- Request connection --
    size_t keep_alive_length = floor(log10(keep_alive)) + 1;
    char command[MQTT_CONNECT_LENGTH_PRE_KEEP_ALIVE + keep_alive_length] = "";

    sprintf(command, MQTT_CONNECT, host, port, keep_alive);
    if (!SequansController.retryCommand(command)) {
        Log.error("Failed to request connection to MQTT broker\r\n");
        return false;
    }

    if (use_tls) {
        if (use_ecc) {
            using_ecc = true;

            uint32_t start = millis();

            while (signIncomingRequests() == false) {
                if (millis() - start > SIGN_TIMEOUT_MS) {
                    Log.error("Timed out waiting for pub signing\r\n");
                    return false;
                }
            }

            delay(100);
        }
    }

    return true;
}

bool MqttClientClass::end(void) {
    SequansController.unregisterCallback(MQTT_ON_MESSAGE_URC);
    SequansController.unregisterCallback(MQTT_ON_CONNECT_URC);
    SequansController.unregisterCallback(MQTT_ON_DISCONNECT_URC);
    SequansController.unregisterCallback(MQTT_ON_PUBLISH_URC);
    SequansController.unregisterCallback(MQTT_ON_SUBSCRIBE_URC);

    connected_callback = NULL;
    disconnected_callback = NULL;

    // Reset variables used for publish and subscribe URC
    got_publish_urc = false;
    publish_status_code = 0;

    got_subscribe_urc = false;
    subscribe_status_code = 0;

    LedCtrl.off(Led::CON, true);

    if (!isConnected()) {
        return true;
    }

    // If LTE is not connected, the MQTT client will be disconnected
    // automatically
    if (!Lte.isConnected()) {
        return true;
    }

    return SequansController.retryCommand(MQTT_DISCONNECT);
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

bool MqttClientClass::publish(const char *topic,
                              const uint8_t *buffer,
                              const uint32_t buffer_size,
                              const MqttQoS quality_of_service) {

    if (!isConnected()) {
        Log.error("Attempted publish without being connected to a broker");
        LedCtrl.off(Led::DATA, false);
        return false;
    }

    if (!SequansController.registerCallback(MQTT_ON_PUBLISH_URC,
                                            internalPublishCallback)) {
        Log.error("Failed to register publish URC");
        return false;
    }

    LedCtrl.on(Led::DATA, true);
    Log.debugf("Starting publishing on topic %s\r\n", topic);

    SequansController.clearReceiveBuffer();

    // Fix for bringing the modem out of idling and prevent timeout whilst
    // waiting for modem response during the next AT command
    SequansController.retryCommand("AT");

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;
    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command,
            MQTT_PUBLISH,
            topic,
            quality_of_service,
            (unsigned long)buffer_size);

    SequansController.writeCommand(command);

    // Wait for start character for delivering payload
    if (SequansController.waitForByte('>', MQTT_TIMEOUT_MS) ==
        SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT) {
        Log.warn("Timed out waiting to deliver MQTT payload.");

        LedCtrl.off(Led::DATA, true);
        SequansController.unregisterCallback(MQTT_ON_PUBLISH_URC);
        return false;
    }

    delay(100);
    SequansController.writeBytes(buffer, buffer_size);

    const uint64_t start = millis();
    while (millis() - start < MQTT_TIMEOUT_MS && !got_publish_urc) {}

    got_publish_urc = false;
    LedCtrl.off(Led::DATA, true);
    SequansController.unregisterCallback(MQTT_ON_PUBLISH_URC);

    if (publish_status_code != 0) {
        Log.errorf("Error happened while publishing: %s\r\n",
                   status_code_table[publish_status_code]);
        return false;
    }

    return true;
}

bool MqttClientClass::publish(const char *topic,
                              const char *message,
                              const MqttQoS quality_of_service) {
    return publish(
        topic, (uint8_t *)message, strlen(message), quality_of_service);
}

bool MqttClientClass::subscribe(const char *topic,
                                const MqttQoS quality_of_service) {

    if (!isConnected()) {
        Log.error(
            "Attempted MQTT Subscribe without being connected to a broker");
        return false;
    }

    if (!SequansController.registerCallback(MQTT_ON_SUBSCRIBE_URC,
                                            internalSubscribeCallback)) {
        Log.error("Failed to register subscribe URC");
        return false;
    }

    SequansController.clearReceiveBuffer();

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic, quality_of_service);

    if (!SequansController.retryCommand(command)) {
        Log.error("Failed to send subscribe command");
        SequansController.unregisterCallback(MQTT_ON_SUBSCRIBE_URC);
        return false;
    }

    const uint64_t start = millis();
    while (millis() - start < MQTT_TIMEOUT_MS && !got_subscribe_urc) {}

    got_subscribe_urc = false;
    SequansController.unregisterCallback(MQTT_ON_SUBSCRIBE_URC);

    if (subscribe_status_code != 0) {
        Log.errorf("Error happened while subscribing: %s\r\n",
                   status_code_table[subscribe_status_code]);
        return false;
    }

    return true;
}

void MqttClientClass::onReceive(void (*callback)(const char *topic,
                                                 const uint16_t message_length,
                                                 const int32_t message_id)) {
    if (callback != NULL) {

        receive_callback = callback;

        SequansController.registerCallback(MQTT_ON_MESSAGE_URC,
                                           internalOnReceiveCallback);
    }
}

bool MqttClientClass::readMessage(const char *topic,
                                  uint8_t *buffer,
                                  const uint16_t buffer_size,
                                  const int32_t message_id) {
    if (buffer_size > MQTT_MAX_BUFFER_SIZE) {

        Log.errorf("MQTT message is longer than the max size of %d\r\n",
                   MQTT_MAX_BUFFER_SIZE);
        return false;
    }

    SequansController.clearReceiveBuffer();

    // Fix for bringing the modem out of idling and prevent timeout whilst
    // waiting for modem response during the next AT command
    SequansController.retryCommand("AT");

    // We determine all message IDs lower than 0 as just no message ID passed
    if (message_id < 0) {
        char command[MQTT_RECEIVE_LENGTH] = "";
        sprintf(command, MQTT_RECEIVE, topic);
        SequansController.writeCommand(command);
    } else {
        const uint32_t digits_in_message_id = trunc(log10(message_id)) + 1;
        char command[MQTT_RECEIVE_WITH_MSG_ID_LENGTH + digits_in_message_id] =
            "";
        sprintf(
            command, MQTT_RECEIVE_WITH_MSG_ID, topic, (unsigned int)message_id);
        SequansController.writeCommand(command);
    }

    // Wait for first byte in receive buffer
    uint32_t start = millis();
    while (!SequansController.isRxReady()) {
        if (millis() - start > MQTT_TIMEOUT_MS) {
            Log.warn(
                "Timed out waiting on the modem to deliver the MQTT message");
            return false;
        }
    }

    // First two bytes are <LF><CR>, so we flush that
    uint8_t start_bytes = 2;

    start = millis();

    while (start_bytes > 0) {
        if (SequansController.readByte() != -1) {
            start_bytes--;
        }

        if (start_bytes > 0 && millis() - start > MQTT_TIMEOUT_MS) {
            Log.warn("Timed out waiting for the modem to deliver the start "
                     "character for the MQTT message");
            return false;
        }
    }

    // Then we retrieve the payload (if any)
    const ResponseResult result =
        SequansController.readResponse((char *)buffer, buffer_size);

    return (result == ResponseResult::OK);
}

String MqttClientClass::readMessage(const char *topic, const uint16_t size) {
    Log.debugf("Reading message on topic %s\r\n", topic);
    // Add bytes for termination of AT command when reading
    char buffer[size + 10];
    if (!readMessage(topic, (uint8_t *)buffer, sizeof(buffer))) {
        return "";
    }

    return buffer;
}

void MqttClientClass::clearMessages(const char *topic,
                                    const uint16_t num_messages) {

    SequansController.clearReceiveBuffer();
    char command[MQTT_RECEIVE_LENGTH] = "";
    sprintf(command, MQTT_RECEIVE, topic);

    for (uint16_t i = 0; i < num_messages; i++) {
        SequansController.writeCommand(command);
    }
}

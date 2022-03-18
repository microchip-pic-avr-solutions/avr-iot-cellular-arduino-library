#include "mqtt_client.h"
#include "ecc608.h"
#include "led_ctrl.h"
#include "log.h"
#include "sequans_controller.h"

#include <cryptoauthlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CONFIGURE         "AT+SQNSMQTTCFG=0,\"%s\""
#define MQTT_CONFIGURE_TLS     "AT+SQNSMQTTCFG=0,\"%s\",,,2"
#define MQTT_CONFIGURE_TLS_ECC "AT+SQNSMQTTCFG=0,\"%s\",,,1"
#define MQTT_CONNECT           "AT+SQNSMQTTCONNECT=0,\"%s\",%u,%u"
#define MQTT_DISCONNECT        "AT+SQNSMQTTDISCONNECT=0"
#define MQTT_PUBLISH           "AT+SQNSMQTTPUBLISH=0,\"%s\",%u,%lu"
#define MQTT_SUSBCRIBE         "AT+SQNSMQTTSUBSCRIBE=0,\"%s\",%u"
#define MQTT_RECEIVE           "AT+SQNSMQTTRCVMESSAGE=0,\"%s\""
#define MQTT_ON_MESSAGE_URC    "SQNSMQTTONMESSAGE"
#define MQTT_ON_CONNECT_URC    "SQNSMQTTONCONNECT"

#define MQTT_ON_DISCONNECT_URC "SQNSMQTTONDISCONNECT"

// Command without any data in it (with parantheses): 19 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 84 bytes
#define MQTT_CONFIGURE_LENGTH 84

#define MQTT_DISCONNECT_LENGTH 24

// Command without any data in it (with parantheses): 22 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 87 bytes
#define MQTT_CONFIGURE_TLS_LENGTH 87

// Command without any data in it (with parantheses): 25 bytes
// Hostname: 127 bytes
// Port: 5 bytes
// Termination: 1 byte
// Total: 157 bytes
#define MQTT_CONNECT_LENGTH_PRE_KEEP_ALIVE 158

// Command without any data in it (with parantheses): 25 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// QoS: 1 byte
// Termination: 1 byte
// Total: 155 bytes
//
// Note that we don't add the length of the data here as it is not fixed
#define MQTT_PUBLISH_LENGTH_PRE_DATA 155

// Command without any data in it (with parantheses): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// QoS: 1 byte
// Termination: 1 byte
// Total: 156 bytes
#define MQTT_SUBSCRIBE_LENGTH 157

// Command without any data in it (with parantheses): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 155 bytes
#define MQTT_RECEIVE_LENGTH 155

// Just arbitrary, but will fit 'ordinary' responses and their termination
#define MQTT_DEFAULT_RESPONSE_LENGTH 256

// This is the index in characters, including delimiter in the connection URC.
#define MQTT_CONNECTION_RC_INDEX  2
#define MQTT_CONNECTION_RC_LENGTH 3

#define MQTT_CONNECTION_SUCCESS_RC '0'

// This is a limitation from the sequans module
#define MQTT_MAX_BUFFER_SIZE 1024

// Max length is 1024, so 4 characters
#define MQTT_MSG_LENGTH_BUFFER_SIZE 4

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

#define POLL_TIMEOUT_MS 20000

// Singleton instance
MqttClientClass MqttClient = MqttClientClass::instance();

static bool connected_to_broker = false;
static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;

volatile static bool signing_request_flag = false;
static char signing_request_buffer[MQTT_SIGNING_BUFFER + 1];
static char signing_urc[URC_DATA_BUFFER_SIZE + 1];

static char receive_urc_buffer[URC_DATA_BUFFER_SIZE + 1];

// +3 since we need two extra characters for the parantheses and one
// extra for null termination in the max case
static char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 3];

static void (*receive_callback)(char *topic, uint16_t message_length) = NULL;

static bool using_ecc = false;

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
    // LedCtrl.off(Led::CON, true);

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

    if (receive_callback != NULL) {
        receive_callback(topic, (uint16_t)atoi(message_length_buffer));
    }
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
        Log.error("no ctx_id\r\r");
        return false;
    }

    // Grab the digest, which will be 32 bytes, but appear as 64 hex
    // characters
    char digest[HCESIGN_DIGEST_LENGTH + 1];

    bool got_digest = SequansController.extractValueFromCommandResponse(
        sign_request, 3, digest, HCESIGN_DIGEST_LENGTH + 1);

    if (!got_digest) {
        Log.error("No Digest\r\n");
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
        Log.error("ECC Signing Failed\r\n");
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

bool MqttClientClass::signIncomingRequests(void) {
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
        Log.error("Could not initialize ECC HW");
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
        Log.error("Could not retrieve thingname from the ECC");
        return false;
    }

    // -- Get the endpoint
    err = ECC608.getEndpoint(endpoint, &endpointLen);

    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve endpoint from the ECC");
        return false;
    }

    Log.debugf("Connecting to AWS with endpoint = %s and thingname = %s\n",
               endpoint,
               thingName);

    using_ecc = true;

    return this->begin(
        (char *)(thingName), (char *)(endpoint), 8883, true, 60, true);
}

bool MqttClientClass::begin(const char *client_id,
                            const char *host,
                            const uint16_t port,
                            const bool use_tls,
                            const size_t keep_alive,
                            const bool use_ecc) {

    // -- Configuration --

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls) {

        char command[MQTT_CONFIGURE_TLS_LENGTH] = "";
        if (use_ecc) {
            sprintf(command, MQTT_CONFIGURE_TLS_ECC, client_id);
        } else {
            sprintf(command, MQTT_CONFIGURE_TLS, client_id);
        }

        if (!SequansController.writeCommand(command)) {
            Log.error("Failed to configure MQTT");
            return false;
        }
    } else {
        char command[MQTT_CONFIGURE_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE, client_id);

        if (!SequansController.writeCommand(command)) {
            Log.error("Failed to configure MQTT");
            return false;
        }
    }

    char conf_resp[MQTT_DEFAULT_RESPONSE_LENGTH * 4];

    ResponseResult result =
        SequansController.readResponse(conf_resp, sizeof(conf_resp));

    if (result != ResponseResult::OK) {
        Log.errorf("Non-OK Response when configuring MQTT. Err = %d\n", result);
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
                if (millis() - start > POLL_TIMEOUT_MS) {
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

    connected_callback = NULL;
    disconnected_callback = NULL;
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

    Log.debugf("Starting publishing on topic %s\r\n", topic);

    if (!isConnected()) {
        Log.error("Attempted MQTT Publish without being connected to a broker");
        return false;
    }

    while (SequansController.isRxReady()) { SequansController.readResponse(); }

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;

    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command, MQTT_PUBLISH, topic, quality_of_service, buffer_size);
    SequansController.writeCommand(command);

    // Wait for start character for delivering payload
    if (SequansController.waitForByte('>', POLL_TIMEOUT_MS) ==
        SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT) {
        Log.error("Timed out waiting for >");
    }
    SequansController.writeBytes(buffer, buffer_size);

    // Wait until we receive the first URC which we discard
    uint8_t wait_result = SequansController.waitForByte(
        URC_IDENTIFIER_START_CHARACTER, POLL_TIMEOUT_MS);

    if (wait_result != SEQUANS_CONTROLLER_READ_BYTE_OK) {
        Log.errorf("Error when waiting for the first URC start character. "
                   "Error was %d\r\n",
                   wait_result);
        return false;
    }

    // Wait until we receive the second URC which includes the status code
    wait_result = SequansController.waitForByte(URC_IDENTIFIER_START_CHARACTER,
                                                POLL_TIMEOUT_MS);
    if (wait_result != SEQUANS_CONTROLLER_READ_BYTE_OK) {
        Log.errorf("Error when waiting for the second URC start character. "
                   "Error was %d\r\n",
                   wait_result);
        return false;
    }

    // We do this as a trick to get an termination sequence after the URC
    SequansController.writeCommand("AT");

    char publish_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    ResponseResult result = SequansController.readResponse(
        publish_response, sizeof(publish_response));

    if (result != ResponseResult::OK) {
        Log.errorf("Failed to get publish result, result was %d \r\n", result);
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    bool got_rc = SequansController.extractValueFromCommandResponse(
        publish_response, 2, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        Log.errorf("Failed to get status code: %s \r\n", rc_buffer);
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        Log.errorf("Status code (rc) != 0: %d\r\n", atoi(rc_buffer));
        return false;
    }

    Log.debug("Published message\r\n");

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

    SequansController.clearReceiveBuffer();

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic, quality_of_service);
    SequansController.writeCommand(command);

    if (SequansController.readResponse() != ResponseResult::OK) {
        Log.error("Failed to write subscribe command");
        return false;
    }

    // Now we wait for the URC
    if (SequansController.waitForByte(URC_IDENTIFIER_START_CHARACTER,
                                      POLL_TIMEOUT_MS) ==
        SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT) {
        Log.error("Timed out waiting for start byte in MQTT subscribe");
        return false;
    }

    // We do this as a trick to get an termination sequence after the URC
    SequansController.writeCommand("AT");

    char subscribe_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (SequansController.readResponse(subscribe_response,
                                       sizeof(subscribe_response)) !=
        ResponseResult::OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    bool got_rc = SequansController.extractValueFromCommandResponse(
        subscribe_response, 2, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        return false;
    }

    return true;
}

void MqttClientClass::onReceive(void (*callback)(char *topic,
                                                 uint16_t message_length)) {
    if (callback != NULL) {

        receive_callback = callback;

        SequansController.registerCallback(MQTT_ON_MESSAGE_URC,
                                           internalOnReceiveCallback);
    }
}

bool MqttClientClass::readMessage(const char *topic,
                                  uint8_t *buffer,
                                  uint16_t buffer_size) {
    if (buffer_size > MQTT_MAX_BUFFER_SIZE) {
        return false;
    }

    SequansController.clearReceiveBuffer();

    char command[MQTT_RECEIVE_LENGTH] = "";
    sprintf(command, MQTT_RECEIVE, topic);
    SequansController.writeCommand(command);

    // Wait for first byte in receive buffer
    uint32_t start = millis();
    while (!SequansController.isRxReady()) {
        if (millis() - start > POLL_TIMEOUT_MS) {
            Log.error("Timed out waiting for reading MQTT message");
            return SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT;
        }
    }

    // First two bytes are <LF><CR>, so we flush that
    uint8_t start_bytes = 2;
    while (start_bytes > 0) {
        if (SequansController.readByte() != -1) {
            start_bytes--;
        }
    }

    // Then we retrieve the payload
    ResponseResult result =
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

bool MqttClientClass::disconnect(bool lte_event) {
    // If we're already disconnected, nothing to do
    if (!isConnected()) {
        return false;
    }

    // If the disconnect event comes from the LTE modem, the
    // AT+SQNSMQTTDISCONNECT command won't work. Override the handler.
    if (lte_event) {
        internalDisconnectCallback(NULL);
        return true;
    }

    // Send the MQTTDISCONNECT command
    SequansController.clearReceiveBuffer();

    char command[MQTT_DISCONNECT_LENGTH] = MQTT_DISCONNECT;
    SequansController.writeCommand(command);

    if (SequansController.readResponse() != ResponseResult::OK) {
        return false;
    }

    // Wait for the broker to disconnect
    uint32_t start = millis();
    while (connected_to_broker) {
        if (millis() - start > 20000) {
            Log.error("Timed out waiting for broker disconnect URC");
            return false;
        }
    }

    return true;
}

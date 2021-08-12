#include "mqtt_client.h"
#include "sequans_controller.h"

#include <cryptoauthlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CONFIGURE         "AT+SQNSMQTTCFG=0,\"%s\""
#define MQTT_CONFIGURE_TLS     "AT+SQNSMQTTCFG=0,\"%s\",\"\",\"\",2"
#define MQTT_CONFIGURE_TLS_ECC "AT+SQNSMQTTCFG=0,\"%s\",\"\",\"\",1"
#define MQTT_CONNECT           "AT+SQNSMQTTCONNECT=0,\"%s\",%u"
#define MQTT_DISCONNECT        "AT+SQNSMQTTDISCONNECT=0"
#define MQTT_PUBLISH           "AT+SQNSMQTTPUBLISH=0,\"%s\",%u,%u"
#define MQTT_SUSBCRIBE         "AT+SQNSMQTTSUBSCRIBE=0,\"%s\",%u"
#define MQTT_RECEIVE           "AT+SQNSMQTTRCVMESSAGE=0,\"%s\""
#define MQTT_ON_MESSAGE_URC    "SQNSMQTTONMESSAGE"
#define MQTT_ON_CONNECT_URC    "SQNSMQTTONCONNECT"
#define MQTT_ON_DISCONNECT_URC "SQNSMQTTONDISCONNECT"
#define HCESIGN                "AT+SQNHCESIGN=%u,0,64,\"%s\""

#define HCESIGN_URC "SQNHCESIGN"

// Command without any data in it (with parantheses): 19 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 84 bytes
#define MQTT_CONFIGURE_LENGTH 84

// Command without any data in it (with parantheses): 27 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 92 bytes
#define MQTT_CONFIGURE_TLS_LENGTH 92

// Command without any data in it (with parantheses): 24 bytes
// Hostname: 127 bytes
// Port: 5 bytes
// Termination: 1 byte
// Total: 157 bytes
#define MQTT_CONNECT_LENGTH 157

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

// Command without any data in it (with parantheses): 22 bytes
// ctxId: 5 bytes (16 bits, thus 5 characters max)
// Signature: 128 bytes
// Termination: 1 byte
// Total: 156 bytes
#define HCESIGN_LENGTH 156

// Just arbitrary, but will fit 'ordinary' responses and their termination
#define MQTT_DEFAULT_RESPONSE_LENGTH 48

// This is the index in characters, including delimiter in the connection URC.
#define MQTT_CONNECTION_RC_INDEX  2
#define MQTT_CONNECTION_RC_LENGTH 3

#define MQTT_CONNECTION_SUCCESS_RC '0'

// This is a limitation from the sequans module
#define MQTT_MAX_BUFFER_SIZE 1024

#define MQTT_TOPIC_MAX_LENGTH 128

// Max length is 1024, so 4 characters
#define MQTT_MSG_LENGTH_BUFFER_SIZE 4

#define HCESIGN_REQUEST_LENGTH 128
#define HCESIGN_DIGEST_LENGTH  64
#define HCESIGN_CTX_ID_LENGTH  5

// Singleton instance
MqttClientClass MqttClient = MqttClientClass::instance();

static bool connected_to_broker = false;
static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;

/**
 * @brief Called on MQTT broker connection URC. Will check the URC to see if the
 * connection was successful.
 */
static void internalConnectedCallback(void) {
    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    if (SequansController.readNotification(rc_buffer, sizeof(rc_buffer)) &&
        rc_buffer[MQTT_CONNECTION_RC_INDEX] == MQTT_CONNECTION_SUCCESS_RC) {

        connected_to_broker = true;

        if (connected_callback != NULL) {
            connected_callback();
        }
    } else {
        connected_to_broker = false;
    }
}

static void internalDisconnectCallback(void) {
    connected_to_broker = true;

    if (disconnected_callback != NULL) {
        disconnected_callback();
    }
}

// TODO: remove all serial prints

bool MqttClientClass::begin(const char *client_id,
                            const char *host,
                            const uint16_t port,
                            const bool use_tls,
                            const bool use_ecc) {
    // We have to make sure we are disconnected first
    SequansController.retryCommand(MQTT_DISCONNECT);
    SequansController.clearReceiveBuffer();

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

        if (!SequansController.retryCommand(command)) {
            return false;
        }

        if (use_ecc) {

            // ECC controller initialization, only for when we are using TLS of
            // course

            // Config for the ECC. This needs to be static since cryptolib
            // defines a pointer to it during the initialization process and
            // stores that for further operations so we don't want to store it
            // on the stack.
            static ATCAIfaceCfg cfg_atecc608b_i2c = {
                ATCA_I2C_IFACE,
                ATECC608B,
                {
                    0x58,  // 7 bit address of ECC
                    2,     // Bus number
                    100000 // Baud rate
                },
                1560,
                20,
                NULL};

            ATCA_STATUS result = atcab_init(&cfg_atecc608b_i2c);

            if (result != ATCA_SUCCESS) {
                return false;
            }
        }
    } else {
        char command[MQTT_CONFIGURE_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE, client_id);

        if (!SequansController.retryCommand(command)) {
            return false;
        }
    }

    SequansController.registerCallback(MQTT_ON_CONNECT_URC,
                                       internalConnectedCallback);
    SequansController.registerCallback(MQTT_ON_DISCONNECT_URC,
                                       internalDisconnectCallback);

    // -- Request connection --
    char command[MQTT_CONNECT_LENGTH] = "";
    sprintf(command, MQTT_CONNECT, host, port);
    if (!SequansController.retryCommand(command)) {
        Serial5.println("Failed set request connection\r\n");
        return false;
    }

    if (use_tls) {

        if (use_ecc) {
            // Wait for sign URC
            while (SequansController.readByte() !=
                   URC_IDENTIFIER_START_CHARACTER) {}

            // Write AT to get an "OK" response which we will search for
            SequansController.writeCommand("AT");

            char sign_request[HCESIGN_REQUEST_LENGTH];
            if (SequansController.readResponse(sign_request,
                                               sizeof(sign_request)) != OK) {

                Serial5.printf("Failed to read response: %s\r\n", sign_request);
                return false;
            }

            Serial5.printf("Got URC: %s\r\n", sign_request);

            if (memcmp(sign_request, "SQNSMQTTON", 10) == 0) {
                Serial5.println("MQTT on connnect, returninig");
                return true;
            }

            // Grab the ctx id
            // +1 for null termination
            char ctx_id_buffer[HCESIGN_CTX_ID_LENGTH + 1];

            bool got_ctx_id = SequansController.extractValueFromCommandResponse(
                sign_request, 0, ctx_id_buffer, sizeof(ctx_id_buffer));

            if (!got_ctx_id) {
                Serial5.printf("Failed to get ctx: %s\r\n", sign_request);
                return false;
            }

            // Grab the digest, which will be 32 bytes, but appear as 64 hex
            // characters
            char digest[HCESIGN_DIGEST_LENGTH + 1];

            bool got_digest = SequansController.extractValueFromCommandResponse(
                sign_request, 3, digest, HCESIGN_DIGEST_LENGTH + 1);

            if (!got_digest) {
                Serial5.printf("Failed to get digest: %s\r\n", sign_request);
                return false;
            }

            // Convert digest to 32 bytes
            uint8_t message_to_sign[HCESIGN_DIGEST_LENGTH / 2];
            char *position = digest;

            for (uint8_t i = 0; i < sizeof(message_to_sign); i++) {
                // TODO: This might be a bit expensive
                sscanf(position, "%2hhx", &message_to_sign[i]);
                position += 2;
            }

            // Sign digest with ECC's primary private key
            ATCA_STATUS result = atcab_sign(0, message_to_sign, digest);

            if (result != ATCA_SUCCESS) {
                Serial5.printf("Failed to sign: %x\r\n", result);
                return false;
            }

            // Now we need to convert the byte array into a hex string in
            // compact form
            const char hex_conversion[] = "0123456789abcdef";
            char command[HCESIGN_LENGTH] = "";

            // +1 for NULL termination
            char signature[HCESIGN_DIGEST_LENGTH * 2 + 1] = "1";

            // Prepare signature by converting to a hex string
            for (uint8_t i = 0; i < sizeof(digest) - 1; i++) {
                signature[i * 2] = hex_conversion[(digest[i] >> 4) & 0x0F];
                signature[i * 2 + 1] = hex_conversion[digest[i] & 0x0F];
            }

            // NULL terminate
            signature[HCESIGN_DIGEST_LENGTH * 2] = 0;
            uint32_t command_length =
                sprintf(command, HCESIGN, atoi(ctx_id_buffer), signature);
            SequansController.writeCommand(command);
            Serial5.printf("Sent command: %s\r\n", command);
        }

        // Wait for MQTT connection URC
        while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER) {
        }

        // Write AT to get an "OK" response which we will search for
        SequansController.writeCommand("AT");

        char connection_response[MQTT_DEFAULT_RESPONSE_LENGTH];
        if (SequansController.readResponse(connection_response,
                                           sizeof(connection_response)) != OK) {

            Serial5.printf("Failed to read connection response: %s\r\n",
                           connection_response);
            return false;
        }

        // +1 for null termination
        char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

        bool got_rc = SequansController.extractValueFromCommandResponse(
            connection_response, 1, rc_buffer, sizeof(rc_buffer));

        if (!got_rc) {
            return false;
        }

        if (atoi(rc_buffer) != 0) {
            return false;
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

    while (SequansController.isRxReady()) { SequansController.readResponse(); }

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;

    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command, MQTT_PUBLISH, topic, quality_of_service, buffer_size);
    SequansController.writeCommand(command);

    // Now we deliver the payload
    SequansController.writeBytes(buffer, buffer_size);
    if (SequansController.readResponse() != OK) {
        return false;
    }

    // Wait until we receive the URC
    while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER) {}

    // We do this as a trick to get an termination sequence after the URC
    SequansController.writeCommand("AT");

    char publish_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    ResponseResult result = SequansController.readResponse(
        publish_response, sizeof(publish_response));

    if (result != OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    bool got_rc = SequansController.extractValueFromCommandResponse(
        publish_response, 0, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        return false;
    }

    return true;
}

bool MqttClientClass::publish(const char *topic,
                              const char *message,
                              const MqttQoS quality_of_service) {

    publish(topic, (uint8_t *)message, strlen(message), quality_of_service);
}

bool MqttClientClass::subscribe(const char *topic,
                                const MqttQoS quality_of_service) {

    SequansController.clearReceiveBuffer();

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic, quality_of_service);
    SequansController.writeCommand(command);

    if (SequansController.readResponse() != OK) {
        return false;
    }

    // Now we wait for the URC
    while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER) {}

    // We do this as a trick to get an termination sequence after the URC
    SequansController.writeCommand("AT");

    char subscribe_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (SequansController.readResponse(subscribe_response,
                                       sizeof(subscribe_response)) != OK) {
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

void MqttClientClass::onReceive(void (*callback)(void)) {
    if (callback != NULL) {
        SequansController.registerCallback(MQTT_ON_MESSAGE_URC, callback);
    }
}

MqttReceiveNotification MqttClientClass::readReceiveNotification(void) {
    MqttReceiveNotification receive_notification{String(), 0};

    // +1 for NULL termination
    char notification_buffer[URC_DATA_BUFFER_SIZE + 1];

    if (!SequansController.readNotification(notification_buffer,
                                            URC_DATA_BUFFER_SIZE)) {
        return receive_notification;
    }

    // +3 since we need two extra characters for the parantheses and one extra
    // for null termination in the max case
    char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 3];

    bool got_topic = SequansController.extractValueFromCommandResponse(
        notification_buffer, 1, topic_buffer, sizeof(topic_buffer), 0);

    if (!got_topic) {
        return receive_notification;
    }

    // Remove parantheses at start and end
    char *topic = topic_buffer + 1;
    topic[strlen(topic) - 1] = 0;

    char message_length_buffer[MQTT_MSG_LENGTH_BUFFER_SIZE + 1];

    bool got_message_length = SequansController.extractValueFromCommandResponse(
        notification_buffer,
        2,
        message_length_buffer,
        sizeof(message_length_buffer),
        0);

    if (!got_message_length) {
        return receive_notification;
    }

    receive_notification.receive_topic = String(topic);
    receive_notification.message_length = (uint16_t)atoi(message_length_buffer);

    return receive_notification;
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
    while (!SequansController.isRxReady()) {}

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

    return (result == OK);
}

String MqttClientClass::readMessage(const char *topic) {
    // TODO: test. Don't think we can do consecutive reads with this, the
    // message might just be discarded after the first read of 128 bytes. The
    // max payload is 1024, but that is a lot of ram to take up.

    // The size is arbitary
    char buffer[128];
    if (!readMessage(topic, (uint8_t *)buffer, sizeof(buffer))) {
        return "";
    }

    return buffer;
}

#include "mqtt_client.h"
#include "sequans_controller.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CONFIGURE      "AT+SQNSMQTTCFG=0,\"%s\""
#define MQTT_CONFIGURE_TLS  "AT+SQNSMQTTCFG=0,\"%s\",\"\",\"\",%u"
#define MQTT_CONNECT        "AT+SQNSMQTTCONNECT=0,\"%s\",%u"
#define MQTT_DISCONNECT     "AT+SQNSMQTTDISCONNECT=0"
#define MQTT_PUBLISH        "AT+SQNSMQTTPUBLISH=0,\"%s\",0,%u"
#define MQTT_SUSBCRIBE      "AT+SQNSMQTTSUBSCRIBE=0,\"%s\",0"
#define MQTT_RECEIVE        "AT+SQNSMQTTRCVMESSAGE=0,\"%s\""
#define MQTT_ON_MESSAGE_URC "SQNSMQTTONMESSAGE"

// Command without any data in it (with parantheses): 19 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 84 bytes
#define MQTT_CONFIGURE_LENGTH 84

// Command without any data in it (with parantheses): 26 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Use TLS: 1 byte
// Termination: 1 byte
// Total: 92 bytes
#define MQTT_CONFIGURE_TLS_LENGTH 92

// Command without any data in it (with parantheses): 24 bytes
// Hostname: 127 bytes
// Port: 5 bytes
// Termination: 1 byte
// Total: 157 bytes
#define MQTT_CONNECT_LENGTH 157

// Command without any data in it (with parantheses): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 155 bytes
//
// Note that we don't add the length of the data here as it is variable
#define MQTT_PUBLISH_LENGTH_PRE_DATA 155

// Command without any data in it (with parantheses): 27 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 156 bytes
#define MQTT_SUBSCRIBE_LENGTH 157

// Command without any data in it (with parantheses): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 155 bytes
#define MQTT_RECEIVE_LENGTH 155

#define MQTT_DEFAULT_RESPONSE_LENGTH 48
#define MQTT_CONNECT_RC_LENGTH       3

// This is a limitation from the sequans module
#define MQTT_MAX_BUFFER_SIZE 1024

#define MQTT_TOPIC_MAX_LENGTH 128

// Max length is 1024, so 4 characters
#define MSG_LENGTH_BUFFER_SIZE 4

#define DEFAULT_RETRIES 5

bool MqttClient::begin(const char *client_id,
                       const char *host,
                       const uint16_t port,
                       const bool use_tls) {
    // We have to make sure we are disconnected first
    end();

    while (SequansController.isRxReady()) { SequansController.flushResponse(); }

    // -- Configuration --

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls) {
        char command[MQTT_CONFIGURE_TLS_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE_TLS, client_id, 1);
        SequansController.writeCommand(command);
    } else {
        char command[MQTT_CONFIGURE_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE, client_id);
        SequansController.writeCommand(command);
    }

    if (SequansController.flushResponse() != OK) {
        return false;
    }

    // -- Connection --

    char command[MQTT_CONNECT_LENGTH] = "";
    sprintf(command, MQTT_CONNECT, host, port);
    SequansController.writeCommand(command);

    if (SequansController.flushResponse() != OK) {
        return false;
    }

    // Now we wait for the URC
    while (!SequansController.isRxReady()) {}

    // Write AT to get an "OK" response which we will search for
    SequansController.writeCommand("AT");

    char connect_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (SequansController.readResponse(connect_response,
                                       sizeof(connect_response)) != OK) {

        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECT_RC_LENGTH + 1];

    bool got_rc = SequansController.extractValueFromCommandResponse(
        connect_response, 1, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        return false;
    }

    return true;
}

bool MqttClient::end(void) {
    SequansController.unregisterCallback(MQTT_ON_MESSAGE_URC);

    uint8_t retry_count = 0;

    do {
        SequansController.writeCommand(MQTT_DISCONNECT);
    } while (SequansController.flushResponse() != OK &&
             retry_count++ < DEFAULT_RETRIES);

    return retry_count < DEFAULT_RETRIES;
}

bool MqttClient::publish(const char *topic,
                         const uint8_t *buffer,
                         const uint32_t buffer_size) {

    while (SequansController.isRxReady()) { SequansController.flushResponse(); }

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;

    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command, MQTT_PUBLISH, topic, buffer_size);
    SequansController.writeCommand(command);

    // Now we deliver the payload
    SequansController.writeBytes(buffer, buffer_size);
    if (SequansController.flushResponse() != OK) {
        return false;
    }

    // Wait until we receive the URC
    while (!SequansController.isRxReady()) {}

    SequansController.writeCommand("AT");

    char publish_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    ResponseResult result = SequansController.readResponse(
        publish_response, sizeof(publish_response));

    if (result != OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECT_RC_LENGTH + 1];

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

bool MqttClient::subscribe(const char *topic) {

    while (SequansController.isRxReady()) { SequansController.flushResponse(); }

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic);
    SequansController.writeCommand(command);

    if (SequansController.flushResponse() != OK) {
        return false;
    }

    // Now we wait for the URC
    while (!SequansController.isRxReady()) {}

    SequansController.writeCommand("AT");

    char subscribe_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (SequansController.readResponse(subscribe_response,
                                       sizeof(subscribe_response)) != OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECT_RC_LENGTH + 1];

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

void MqttClient::registerReceiveNotificationCallback(void (*callback)(void)) {
    SequansController.registerCallback(MQTT_ON_MESSAGE_URC, callback);
}

MqttReceiveNotification MqttClient::readReceiveNotification(void) {
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

    char message_length_buffer[MSG_LENGTH_BUFFER_SIZE + 1];

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

bool MqttClient::readMessage(const char *topic,
                             uint8_t *buffer,
                             uint16_t buffer_size) {
    if (buffer_size > MQTT_MAX_BUFFER_SIZE) {
        return false;
    }

    while (SequansController.isRxReady()) { SequansController.flushResponse(); }

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

#include "mqtt_client.h"
#include "sequans_controller.h"

#include <Arduino.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CONFIGURE     "AT+SQNSMQTTCFG=0,\"%s\""
#define MQTT_CONFIGURE_TLS "AT+SQNSMQTTCFG=0,\"%s\",\"\",\"\",%u"
#define MQTT_CONNECT       "AT+SQNSMQTTCONNECT=0,\"%s\",%u"
#define MQTT_DISCONNECT    "AT+SQNSMQTTDISCONNECT=0"
#define MQTT_PUBLISH       "AT+SQNSMQTTPUBLISH=0,\"%s\",%u,%u"
#define MQTT_SUSBCRIBE     "AT+SQNSMQTTSUBSCRIBE=0,\"%s\",%u"

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

// Command without any data in it (with parantheses): 25 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// QoS: 1 byte
// Termination: 1 byte
// Total: 155 bytes
//
// Note that we don't add the length of the data here as it is variable
#define MQTT_PUBLISH_LENGTH_PRE_DATA 155

// Command without any data in it (with parantheses): 26 bytes
// Topic: 128 bytes (this is imposed by this implementation)
// QoS: 1 byte
// Termination: 1 byte
// Total: 156 bytes
#define MQTT_SUBSCRIBE_LENGTH 157

#define MQTT_DEFAULT_RESPONSE_LENGTH 48
#define MQTT_CONNECT_RC_LENGTH       3

bool mqttClientConfigure(const char *client_id, const bool use_tls) {

    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // We have to make sure we are disconnected first
    mqttClientDisconnect();

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls) {
        char command[MQTT_CONFIGURE_TLS_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE_TLS, client_id, 1);
        sequansControllerWriteCommand(command);
    } else {
        char command[MQTT_CONFIGURE_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE, client_id);
        sequansControllerWriteCommand(command);
    }

    return (sequansControllerFlushResponse() == OK);
}

bool mqttClientConnect(const char *host, const uint16_t port) {

    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    char command[MQTT_CONNECT_LENGTH] = "";
    sprintf(command, MQTT_CONNECT, host, port);
    sequansControllerWriteCommand(command);

    if (sequansControllerFlushResponse() != OK) {
        return false;
    }

    // Now we wait for the URC
    // TODO: use RING0?
    while (!sequansControllerIsRxReady()) {}

    // Write AT to get an "OK" response which we will search for
    sequansControllerWriteCommand("AT");

    char connect_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (sequansControllerReadResponse(connect_response,
                                      sizeof(connect_response)) != OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECT_RC_LENGTH + 1];

    bool got_rc = sequansControllerExtractValueFromCommandResponse(
        connect_response, 1, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        return false;
    }

    return true;
}

bool mqttClientDisconnect(void) {
    sequansControllerWriteCommand(MQTT_DISCONNECT);
    return (sequansControllerFlushResponse() == OK);
}

bool mqttClientPublish(const char *topic,
                       const uint8_t qos,
                       const uint8_t *buffer,
                       const uint32_t buffer_size) {

    // Flush receive buffer if there
    // TODO: URC
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;

    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command, MQTT_PUBLISH, topic, qos, buffer_size);
    sequansControllerWriteCommand(command);

    // Now we deliver the payload
    sequansControllerWriteBytes(buffer, buffer_size);
    if (sequansControllerFlushResponse() != OK) {
        return false;
    }

    // Wait until we receive the URC
    // TODO: URC
    while (!sequansControllerIsRxReady()) {}

    sequansControllerWriteCommand("AT");

    char publish_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    ResponseResult result = sequansControllerReadResponse(
        publish_response, sizeof(publish_response));

    if (result != OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECT_RC_LENGTH + 1];

    bool got_rc = sequansControllerExtractValueFromCommandResponse(
        publish_response, 0, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        return false;
    }

    return true;
}

bool mqttClientSubscribe(const char *topic, const uint8_t qos) {

    // Flush receive buffer if there
    // TODO: URC
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic, qos);
    sequansControllerWriteCommand(command);

    if (sequansControllerFlushResponse() != OK) {
        return false;
    }

    // Now we wait for the URC
    // TODO: use RING0?
    while (!sequansControllerIsRxReady()) {}

    sequansControllerWriteCommand("AT");

    char subscribe_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (sequansControllerReadResponse(subscribe_response,
                                      sizeof(subscribe_response)) != OK) {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECT_RC_LENGTH + 1];

    bool got_rc = sequansControllerExtractValueFromCommandResponse(
        subscribe_response, 2, rc_buffer, sizeof(rc_buffer));

    if (!got_rc) {
        return false;
    }

    if (atoi(rc_buffer) != 0) {
        return false;
    }

    return true;
}

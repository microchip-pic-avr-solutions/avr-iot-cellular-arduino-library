#include "mqtt_client.h"
#include "sequans_controller.h"
#include "ecc608/ecc608.h"
#include "log/log.h"

#include <cryptoauthlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CONFIGURE "AT+SQNSMQTTCFG=0,\"%s\""
#define MQTT_CONFIGURE_TLS "AT+SQNSMQTTCFG=0,\"%s\",,,2"
#define MQTT_CONFIGURE_TLS_ECC "AT+SQNSMQTTCFG=0,\"%s\",,,1"
#define MQTT_CONNECT "AT+SQNSMQTTCONNECT=0,\"%s\",%u"
#define MQTT_DISCONNECT "AT+SQNSMQTTDISCONNECT=0"
#define MQTT_PUBLISH "AT+SQNSMQTTPUBLISH=0,\"%s\",%u,%lu"
#define MQTT_SUSBCRIBE "AT+SQNSMQTTSUBSCRIBE=0,\"%s\",%u"
#define MQTT_RECEIVE "AT+SQNSMQTTRCVMESSAGE=0,\"%s\""
#define MQTT_ON_MESSAGE_URC "SQNSMQTTONMESSAGE"
#define MQTT_ON_CONNECT_URC "SQNSMQTTONCONNECT"
#define MQTT_ON_DISCONNECT_URC "SQNSMQTTONDISCONNECT"

// Command without any data in it (with parantheses): 19 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 84 bytes
#define MQTT_CONFIGURE_LENGTH 84

// Command without any data in it (with parantheses): 22 bytes
// Client ID: 64 bytes (this is imposed by this implementation)
// Termination: 1 byte
// Total: 87 bytes
#define MQTT_CONFIGURE_TLS_LENGTH 87

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

// Just arbitrary, but will fit 'ordinary' responses and their termination
#define MQTT_DEFAULT_RESPONSE_LENGTH 256

// This is the index in characters, including delimiter in the connection URC.
#define MQTT_CONNECTION_RC_INDEX 2
#define MQTT_CONNECTION_RC_LENGTH 3

#define MQTT_CONNECTION_SUCCESS_RC '0'

// This is a limitation from the sequans module
#define MQTT_MAX_BUFFER_SIZE 1024

#define MQTT_TOPIC_MAX_LENGTH 128

// Max length is 1024, so 4 characters
#define MQTT_MSG_LENGTH_BUFFER_SIZE 4

#define MQTT_SIGNING_BUFFER 256

#define HCESIGN_URC "SQNHCESIGN"

// Singleton instance
MqttClientClass MqttClient = MqttClientClass::instance();

static bool connected_to_broker = false;
static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;

volatile static bool signingRequestFlag = false;
static char signingRequestBuffer[MQTT_SIGNING_BUFFER];

static bool usingEcc = false;

/**
 * @brief Called on MQTT broker connection URC. Will check the URC to see if the
 * connection was successful.
 */
static void internalConnectedCallback(char *urc)
{
    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    if (SequansController.readNotification(rc_buffer, sizeof(rc_buffer)) &&
        rc_buffer[MQTT_CONNECTION_RC_INDEX] == MQTT_CONNECTION_SUCCESS_RC)
    {

        connected_to_broker = true;

        if (connected_callback != NULL)
        {
            connected_callback();
        }
    }
    else
    {
        connected_to_broker = false;
    }
}

static void internalDisconnectCallback(char *urc)
{
    connected_to_broker = true;

    if (disconnected_callback != NULL)
    {
        disconnected_callback();
    }
}

static void internalHandleSigningRequest(char *urc)
{
    bool ret = SequansController.genSigningRequestCmd(urc, signingRequestBuffer);
    if (ret != true)
    {
        Log5.Error("Unable to handle signature request");
        return;
    }
    signingRequestFlag = true;
}

// Returns true if a signing was done
bool MqttClientClass::pollSign(void)
{
    bool ret = false;
    if (signingRequestFlag)
    {
        Log5.Debug("Signing");
        ret = SequansController.writeCommand(signingRequestBuffer);
        signingRequestFlag = false;
    }

    return ret;
}

bool MqttClientClass::beginAWS()
{
    // Get the endoint and thing name
    // -- Initialize the ECC
    uint8_t err = ECC608.initializeHW();
    if (err != ATCA_SUCCESS)
    {
        Log5.Error("Could not initialize ECC HW");
        return false;
    }

    // -- Allocate the buffers
    uint8_t thingName[128];
    uint8_t thingNameLen = sizeof(thingName);
    uint8_t endpoint[128];
    uint8_t endpointLen = sizeof(endpoint);

    // -- Get the thingname
    err = ECC608.getThingName(thingName, &thingNameLen);
    if (err != ECC608.ERR_OK)
    {
        Log5.Error("Could not retrieve thingname from the ECC");
        return false;
    }

    // -- Get the endpoint
    err = ECC608.getEndpoint(endpoint, &endpointLen);
    if (err != ECC608.ERR_OK)
    {
        Log5.Error("Could not retrieve endpoint from the ECC");
        return false;
    }

    Log5.Debugf("Connecting to AWS with endpoint = %s and thingname = %s\n", endpoint, thingName);

    usingEcc = true;

    return this->begin((char *)(thingName), (char *)(endpoint), 8883, true, true);
}

bool MqttClientClass::begin(const char *client_id,
                            const char *host,
                            const uint16_t port,
                            const bool use_tls,
                            const bool use_ecc)
{
    // We have to make sure we are disconnected first
    SequansController.writeCommand(MQTT_DISCONNECT);
    SequansController.clearReceiveBuffer();

    // -- Configuration --

    // The sequans modem fails if we specify 0 as TLS, so we just have to have
    // two commands for this
    if (use_tls)
    {

        char command[MQTT_CONFIGURE_TLS_LENGTH] = "";
        if (use_ecc)
        {
            sprintf(command, MQTT_CONFIGURE_TLS_ECC, client_id);
        }
        else
        {
            sprintf(command, MQTT_CONFIGURE_TLS, client_id);
        }

        if (!SequansController.retryCommand(command))
        {
            return false;
        }
    }
    else
    {
        char command[MQTT_CONFIGURE_LENGTH] = "";
        sprintf(command, MQTT_CONFIGURE, client_id);

        if (!SequansController.writeCommand(command))
        {
            Log5.Error("Failed to configure MQTT");
            return false;
        }
    }

    SequansController.registerCallback(MQTT_ON_CONNECT_URC,
                                       internalConnectedCallback);
    SequansController.registerCallback(MQTT_ON_DISCONNECT_URC,
                                       internalDisconnectCallback);
    SequansController.registerCallback(HCESIGN_URC, internalHandleSigningRequest);

    // -- Request connection --
    char command[MQTT_CONNECT_LENGTH] = "";

    sprintf(command, MQTT_CONNECT, host, port);
    if (!SequansController.retryCommand(command))
    {
        Log5.Error("Failed to request connection to MQTT broker\r\n");
        return false;
    }

    if (use_tls)
    {

        if (use_ecc)
        {
            usingEcc = true;

            while (pollSign() == false)
                ;
            delay(1000);
        }

        // Wait for MQTT connection URC
        while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER)
        {
        }

        // Write AT to get an "OK" response which we will search for
        SequansController.writeCommand("AT");

        char connection_response[MQTT_DEFAULT_RESPONSE_LENGTH * 4];
        uint8_t res = SequansController.readResponse(connection_response,
                                                     sizeof(connection_response));
        if (res != OK)
        {
            Log5.Errorf("Non-OK Response when writing AT. Err = %d\n", res);
            return false;
        }

        // +1 for null termination
        char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

        bool got_rc = SequansController.extractValueFromCommandResponse(
            connection_response, 1, rc_buffer, sizeof(rc_buffer));

        if (!got_rc)
        {
            return false;
        }

        if (atoi(rc_buffer) != 0)
        {
            return false;
        }
    }

    return true;
}

bool MqttClientClass::end(void)
{
    SequansController.unregisterCallback(MQTT_ON_MESSAGE_URC);
    SequansController.unregisterCallback(MQTT_ON_CONNECT_URC);
    SequansController.unregisterCallback(MQTT_ON_DISCONNECT_URC);

    connected_callback = NULL;
    disconnected_callback = NULL;
    return SequansController.retryCommand(MQTT_DISCONNECT);
}

void MqttClientClass::onConnectionStatusChange(void (*connected)(void),
                                               void (*disconnected)(void))
{
    if (connected != NULL)
    {
        connected_callback = connected;
    }

    if (disconnected != NULL)
    {
        disconnected_callback = disconnected;
    }
}

bool MqttClientClass::isConnected(void)
{
    return connected_to_broker;
}

bool MqttClientClass::publish(const char *topic,
                              const uint8_t *buffer,
                              const uint32_t buffer_size,
                              const MqttQoS quality_of_service)
{
    while (SequansController.isRxReady())
    {
        SequansController.readResponse();
    }

    const size_t digits_in_buffer_size = trunc(log10(buffer_size)) + 1;

    char command[MQTT_PUBLISH_LENGTH_PRE_DATA + digits_in_buffer_size];

    // Fill everything besides the data
    sprintf(command, MQTT_PUBLISH, topic, quality_of_service, buffer_size);
    SequansController.writeCommand(command);

    // Wait for start character for delivering payload
    while (SequansController.readByte() != '>')
    {
    }
    SequansController.writeBytes(buffer, buffer_size);

    // Wait until we receive the first URC which we discard
    while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER)
    {
    }

    // Wait until we receive the second URC which includes the status code
    while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER)
    {
    }

    // Wait for a signing request if using the ECC
    if (usingEcc)
    {
        uint32_t start = millis();
        while (pollSign() == false)
        {
            if (millis() - start > 5000)
            {
                Log5.Error("Timed out waiting for pub signing");
                return false;
            }
        }
    }

    // We do this as a trick to get an termination sequence after the URC
    SequansController.writeCommand("AT");

    char publish_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    ResponseResult result = SequansController.readResponse(
        publish_response, sizeof(publish_response));

    if (result != OK)
    {
        Log5.Errorf("Failed to get publish result, result was %d\n", result);
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    bool got_rc = SequansController.extractValueFromCommandResponse(
        publish_response, 2, rc_buffer, sizeof(rc_buffer));

    if (!got_rc)
    {
        Log5.Errorf("Failed to get status code: %s \r\n", rc_buffer);
        return false;
    }

    if (atoi(rc_buffer) != 0)
    {
        Log5.Errorf("Status code (rc) != 0: %d\r\n", atoi(rc_buffer));
        return false;
    }

    return true;
}

bool MqttClientClass::publish(const char *topic,
                              const char *message,
                              const MqttQoS quality_of_service)
{
    return publish(
        topic, (uint8_t *)message, strlen(message), quality_of_service);
}

bool MqttClientClass::subscribe(const char *topic,
                                const MqttQoS quality_of_service)
{
    SequansController.clearReceiveBuffer();

    char command[MQTT_SUBSCRIBE_LENGTH] = "";
    sprintf(command, MQTT_SUSBCRIBE, topic, quality_of_service);
    SequansController.writeCommand(command);

    if (SequansController.readResponse() != OK)
    {
        return false;
    }

    // Now we wait for the URC
    while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER)
    {
    }

    // We do this as a trick to get an termination sequence after the URC
    SequansController.writeCommand("AT");

    char subscribe_response[MQTT_DEFAULT_RESPONSE_LENGTH];
    if (SequansController.readResponse(subscribe_response,
                                       sizeof(subscribe_response)) != OK)
    {
        return false;
    }

    // +1 for null termination
    char rc_buffer[MQTT_CONNECTION_RC_LENGTH + 1];

    bool got_rc = SequansController.extractValueFromCommandResponse(
        subscribe_response, 2, rc_buffer, sizeof(rc_buffer));

    if (!got_rc)
    {
        return false;
    }

    if (atoi(rc_buffer) != 0)
    {
        return false;
    }

    return true;
}

void MqttClientClass::onReceive(void (*callback)(char *))
{
    if (callback != NULL)
    {
        SequansController.registerCallback(MQTT_ON_MESSAGE_URC, callback);
    }
}

MqttReceiveNotification MqttClientClass::readReceiveNotification(void)
{
    MqttReceiveNotification receive_notification{String(), 0};

    // +1 for NULL termination
    char notification_buffer[URC_DATA_BUFFER_SIZE + 1];

    if (!SequansController.readNotification(notification_buffer,
                                            URC_DATA_BUFFER_SIZE))
    {
        return receive_notification;
    }

    // +3 since we need two extra characters for the parantheses and one extra
    // for null termination in the max case
    char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 3];

    bool got_topic = SequansController.extractValueFromCommandResponse(
        notification_buffer, 1, topic_buffer, sizeof(topic_buffer), 0);

    if (!got_topic)
    {
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

    if (!got_message_length)
    {
        return receive_notification;
    }

    receive_notification.receive_topic = String(topic);
    receive_notification.message_length = (uint16_t)atoi(message_length_buffer);

    return receive_notification;
}

bool MqttClientClass::readMessage(const char *topic,
                                  uint8_t *buffer,
                                  uint16_t buffer_size)
{
    if (buffer_size > MQTT_MAX_BUFFER_SIZE)
    {
        return false;
    }

    SequansController.clearReceiveBuffer();

    char command[MQTT_RECEIVE_LENGTH] = "";
    sprintf(command, MQTT_RECEIVE, topic);
    SequansController.writeCommand(command);

    // Wait for first byte in receive buffer
    while (!SequansController.isRxReady())
    {
    }

    // First two bytes are <LF><CR>, so we flush that
    uint8_t start_bytes = 2;
    while (start_bytes > 0)
    {
        if (SequansController.readByte() != -1)
        {
            start_bytes--;
        }
    }

    // Then we retrieve the payload
    ResponseResult result =
        SequansController.readResponse((char *)buffer, buffer_size);

    return (result == OK);
}

String MqttClientClass::readMessage(const char *topic, const uint16_t size)
{
    // Add bytes for termination of AT command when reading
    char buffer[size + 10];
    if (!readMessage(topic, (uint8_t *)buffer, sizeof(buffer)))
    {
        return "";
    }

    return buffer;
}

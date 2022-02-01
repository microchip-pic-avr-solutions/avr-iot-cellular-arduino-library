/**
 * This example uses polling for the LTE module and the MQTT module when
 * checking for new messages.
 */

#include <Arduino.h>

#include <lte.h>
#include <mqtt_client.h>
#include "ecc608/ecc608.h"
#include "log/log.h"

#define MQTT_USE_AWS false
#define MQTT_SUB_TOPIC "mchp_topic_sub"
#define MQTT_PUB_TOPIC "mchp_topic_pub"

// If you are not using AWS, apply these settings
#if (!MQTT_USE_AWS)

#define MQTT_THING_NAME "someuniquemchp"
#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_USE_TLS false
#define MQTT_USE_ECC false

#endif

bool connectedToBroker = false;

void setup()
{
    Serial3.begin(115200);
    Log3.setLogLevel(LogLevels::INFO);
    Log3.Info("Starting initialization of MQTT Polling");

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected())
    {
        Log3.Info("Not connected to operator yet...");
        delay(5000);
    }

    Log3.Info("Connected to operator!");

// Attempt to connect to broker
#if (MQTT_USE_AWS)
    connectedToBroker = MqttClient.beginAWS();
#else
    connectedToBroker = MqttClient.begin(
        MQTT_THING_NAME, MQTT_BROKER, MQTT_PORT, MQTT_USE_TLS, MQTT_USE_ECC);
#endif

    if (connectedToBroker)
    {
        Log3.Info("Connecting to broker...");
        while (!MqttClient.isConnected())
        {
            Log3.Info("Connecting...");
            delay(500);
        }
        Log3.Info("Connected to broker!");
        MqttClient.subscribe(MQTT_SUB_TOPIC);
    }
    else
    {
        Log3.Error("Failed to connect to broker");
    }
}

void loop()
{

    if (connectedToBroker)
    {

        String message = MqttClient.readMessage(MQTT_SUB_TOPIC);

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a new
        // message
        if (message != "")
        {
            Log3.Info("Got new message: ");
            Log3.Info(message);
        }

// Publishing can fail due to network issues, so to be on the safe side
// one should check the return value to see if the message got published
#if ((MQTT_USE_ECC) || (MQTT_USE_AWS))
        // If we are using the ECC (secure element), we need to poll for situations where the Sequans modem wants something signed.
        MqttClient.pollSign();
#endif

        bool publishedSuccessfully =
            MqttClient.publish(MQTT_PUB_TOPIC, "hello world");

        if (!publishedSuccessfully)
        {
            Log3.Error("Failed to publish");
        }
    }

    delay(2000);
}

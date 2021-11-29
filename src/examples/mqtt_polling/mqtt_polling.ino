/**
 * This example uses polling for the LTE module and the MQTT module when
 * checking for new messages.
 */

#include <Arduino.h>
#include <lte.h>
#include <mqtt_client.h>

//#define MQTT_THING_NAME "cccf626c3be836af9f72fb534b42b3ea4cc6e1dd"
#define MQTT_THING_NAME "basicPubSub"
#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_USE_TLS false
#define MQTT_USE_ECC false

#define SerialDebug Serial5

bool connectedToBroker = false;

void setup()
{
    SerialDebug.begin(115200);
    SerialDebug.println("Starting initialization of MQTT Polling");

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected())
    {
        SerialDebug.println("Not connected to operator yet...");
        delay(5000);
    }

    SerialDebug.println("Connected to operator!");

    // Attempt to connect to broker
    connectedToBroker = MqttClient.begin(
        MQTT_THING_NAME, MQTT_BROKER, MQTT_PORT, MQTT_USE_TLS, MQTT_USE_ECC);

    if (connectedToBroker)
    {
        SerialDebug.println("Connecting to broker...");
        while (!MqttClient.isConnected())
        {
            SerialDebug.println("Connecting...");
            delay(500);
        }
        SerialDebug.println("Connected to broker!");
        MqttClient.subscribe("mchp_topic");
    }
    else
    {
        SerialDebug.println("Failed to connect to broker");
    }
}

void loop()
{

    if (connectedToBroker)
    {

        SerialDebug.println("reading message");
        String message = MqttClient.readMessage("mchp_topic");

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a new
        // message
        if (message != "")
        {
            SerialDebug.print("Got new message: ");
            SerialDebug.println(message);
        }

        // Publishing can fail due to network issues, so to be on the safe side
        // one should check the return value to see if the message got published
        bool publishedSuccessfully =
            MqttClient.publish("topic_2", "hello world");

        if (!publishedSuccessfully)
        {
            SerialDebug.println("Failed to publish");
        }
    }

    MqttClient.pollSign();
}

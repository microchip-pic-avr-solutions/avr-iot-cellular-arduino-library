#include <lte.h>
#include <mqtt_client.h>

#include <Arduino.h>

#define MQTT_THING_NAME "0b34785df53a3f9c88304c1c6b5e692b1dd6d958"
#define MQTT_BROKER     "a2o6d3azuiiax4-ats.iot.us-east-2.amazonaws.com"
#define MQTT_PORT       8883
#define MQTT_USE_TLS    true
#define MQTT_USE_ECC    false

#define SerialDebug Serial5

bool connectedToBroker = false;

void setup() {
    SerialDebug.begin(115200);
    SerialDebug.println("Starting initialization");

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
    while (!Lte.isConnected()) {
        SerialDebug.println("Not connected to operator yet...");
        delay(5000);
    }

    SerialDebug.println("Connected!");

    // Attempt to connect to broker
    connectedToBroker = MqttClient.begin(
        MQTT_THING_NAME, MQTT_BROKER, MQTT_PORT, MQTT_USE_TLS, MQTT_USE_ECC);

    if (connectedToBroker) {
        Serial5.println("Connected to broker!");
        MqttClient.subscribe("frombroker");
    } else {
        SerialDebug.println("Failed to connect to broker");
    }
}

void loop() {

    if (connectedToBroker) {

        String message = MqttClient.readMessage("frombroker");

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a new
        // message
        if (message != "") {
            SerialDebug.print("Got new message: ");
            SerialDebug.println(message);
        }

        // Publishing can fail due to network issues, so to be on the safe side
        // one should check the return value to see if the message got published
        bool publishedSuccessfully =
            MqttClient.publish("tobroker", "hello world");

        if (!publishedSuccessfully) {
            SerialDebug.println("Failed to publish");
        }
    }

    delay(2000);
}

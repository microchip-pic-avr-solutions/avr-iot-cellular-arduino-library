#include <lte.h>
#include <mqtt_client.h>

#include <Arduino.h>

#define MQTT_THING_NAME "avrdb64"
#define MQTT_BROKER     "a2o6d3azuiiax4-ats.iot.us-east-2.amazonaws.com"
#define MQTT_PORT       8883
#define MQTT_USE_TLS    true

#define SerialDebug Serial5

bool connectedToBroker = false;

void setup() {
    SerialDebug.begin(115200);

    // Start LTE modem and wait until we are connected
    Lte.begin();
    while (!Lte.isConnected()) { delay(1000); }

    // Connect to broker and subscribe to topic if we manage to connect
    connectedToBroker =
        MqttClient.begin(MQTT_THING_NAME, MQTT_BROKER, MQTT_PORT, MQTT_USE_TLS);

    if (connectedToBroker) {
        MqttClient.subscribe("frombroker");
    } else {
        SerialDebug.println("Failed to connect to broker");
    }
}

void loop() {

    if (connectedToBroker) {

        String message = MqttClient.readMessage("frombroker");

        if (message != "") {
            SerialDebug.print("Got new message: ");
            SerialDebug.println(message);
        }

        bool publishedSuccessfully =
            MqttClient.publish("tobroker", "hello world");

        if (!publishedSuccessfully) {
            SerialDebug.println("Failed to publish");
        }
    }

    delay(2000);
}

/**
 * This example uses polling for the LTE module and the MQTT module when
 * checking for new messages.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>

#define MQTT_SUB_TOPIC "mchp_topic"
#define MQTT_PUB_TOPIC "mchp_topic"

#define MQTT_THING_NAME "someuniquemchp"
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_PORT       1883
#define MQTT_USE_TLS    false
#define MQTT_USE_ECC    false
#define MQTT_KEEPALIVE  60

volatile bool lteConnected = false;
volatile bool mqttConnected = false;

void mqttDisconnectHandler() { mqttConnected = false; }

void lteDisconnectHandler() { lteConnected = false; }

void connectMqtt() {
    MqttClient.onConnectionStatusChange(NULL, mqttDisconnectHandler);

    mqttConnected = MqttClient.begin(MQTT_THING_NAME,
                                     MQTT_BROKER,
                                     MQTT_PORT,
                                     MQTT_USE_TLS,
                                     MQTT_KEEPALIVE,
                                     MQTT_USE_ECC);

    if (mqttConnected) {
        Log.info("Connecting to broker...");
        while (!MqttClient.isConnected()) {
            Log.info("Connecting...");
            delay(500);

            // If we're not connected to the network, give up
            if (!lteConnected) {
                return;
            }
        }

        Log.info("Connected to broker!");

        MqttClient.subscribe(MQTT_SUB_TOPIC);
    } else {
        Log.error("Failed to connect to broker");
    }
}

void connectLTE() {

    Lte.onConnectionStatusChange(NULL, lteDisconnectHandler);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Not connected to operator yet...");
        delay(5000);
    }

    Log.info("Connected to operator!");
    lteConnected = true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting initialization of MQTT Polling");
}

static uint32_t counter = 0;

void loop() {

    if (mqttConnected) {

        String message = MqttClient.readMessage(MQTT_SUB_TOPIC);

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a new
        // message
        if (message != "") {
            Log.infof("Got new message: %s\r\n", message.c_str());
        }

        String message_to_publish = String("Hello world: " + String(counter));

        bool publishedSuccessfully =
            MqttClient.publish(MQTT_PUB_TOPIC, message_to_publish.c_str());

        if (publishedSuccessfully) {
            Log.infof("Published message: %s\r\n", message_to_publish.c_str());
            counter++;
        } else {
            Log.error("Failed to publish");
        }
    } else {
        // MQTT is not connected. Need to establish connection

        if (!lteConnected) {
            // We're NOT connected to the LTE Network. Establish LTE connection
            // first
            Log.info("LTE is not connected. Establishing...");
            connectLTE();
        }

        connectMqtt();
    }

    delay(1000);
}

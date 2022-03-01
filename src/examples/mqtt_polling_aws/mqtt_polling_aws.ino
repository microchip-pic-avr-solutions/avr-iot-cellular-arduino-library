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

// AWS defines which topic you are allowed to subscribe and publish too. This is
// defined by the policy The default policy with the Microchip IoT Provisioning
// Tool allows for publishing and subscribing on thing_id/topic. If you want to
// publish and subscribe on other topics, see the AWS IoT Core Policy
// documentation.
#define MQTT_SUB_TOPIC_FMT "%s/mchp_topic_sub"
#define MQTT_PUB_TOPIC_FMT "%s/sensors"

char mqtt_sub_topic[128];
char mqtt_pub_topic[128];

volatile bool lteConnected = false;
volatile bool mqttConnected = false;

void mqttDCHandler() { mqttConnected = false; }

void lteDCHandler() { lteConnected = false; }

void connectMqtt() {
    MqttClient.onConnectionStatusChange(NULL, mqttDCHandler);

    // Attempt to connect to broker
    mqttConnected = MqttClient.beginAWS();

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

        Log.info("Connected to broker!\r\n");

        // Subscribe to the topic
        MqttClient.subscribe(mqtt_sub_topic);
    } else {
        Log.error("Failed to connect to broker\r\n");
    }
}

void connectLTE() {

    Lte.onConnectionStatusChange(NULL, lteDCHandler);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Not connected to operator yet...\r\n");
        delay(5000);
    }

    Log.info("Connected to operator!\r\n");
    lteConnected = true;
}

bool initMQTTTopics() {
    ECC608.begin();

    // Find the thing ID and set the publish and subscription topics
    uint8_t thingName[128];
    uint8_t thingNameLen = sizeof(thingName);

    // -- Get the thingname
    uint8_t err = ECC608.getThingName(thingName, &thingNameLen);
    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve thingname from the ECC");
        return false;
    }

    sprintf(mqtt_sub_topic, MQTT_SUB_TOPIC_FMT, thingName);
    sprintf(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, thingName);

    return true;
}

void setup() {
    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting initialization of MQTT Polling for AWS\r\n");

    if (initMQTTTopics() == false) {
        Log.error("Unable to initialize the MQTT topics. Stopping...");
        while (1)
            ;
    }

    connectLTE();
    connectMqtt();
}

void loop() {

    if (mqttConnected) {
        String message = MqttClient.readMessage(mqtt_sub_topic);

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a
        // new message

        if (message != "") {
            Log.info("Got new message: ");
            Log.info(message);
            Log.info("\r\n");
        }

        bool publishedSuccessfully =
            MqttClient.publish(mqtt_pub_topic, "{\"light\": 9, \"temp\": 9}");

        if (!publishedSuccessfully) {
            Log.error("Failed to publish\r\n");
        }
    } else {
        // MQTT is not connected. Need to re-establish connection
        if (!lteConnected) {
            // We're NOT connected to the LTE Network. Establish LTE connection
            // first
            connectLTE();
        }

        connectMqtt();
    }
    delay(1000);
}

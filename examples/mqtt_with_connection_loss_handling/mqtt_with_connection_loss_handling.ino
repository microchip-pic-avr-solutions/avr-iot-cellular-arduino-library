/**
 * @brief This example demonstrates a more robust MQTT program which takes into
 * consideration network disconnection and broker disconnection.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>
#include <sequans_controller.h>

#define MQTT_PUB_TOPIC_FMT "%s/sensors"

static char mqtt_pub_topic[128];

static volatile bool connecteded_to_network = false;
static volatile bool connected_to_broker    = false;

void disconnectedFromBroker(void) { connected_to_broker = false; }

void disconnectedFromNetwork(void) { connecteded_to_network = false; }

bool initMQTTTopics() {
    ECC608.begin();

    // Find the thing ID and set the publish and subscription topics
    uint8_t thingName[128];
    size_t thingNameLen = sizeof(thingName);

    // -- Get the thingname
    ATCA_STATUS status =
        ECC608.readProvisionItem(AWS_THINGNAME, thingName, &thingNameLen);
    if (status != ATCA_SUCCESS) {
        Log.error(F("Could not retrieve thingname from the ECC"));
        return false;
    }

    sprintf(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, thingName);

    return true;
}

static bool connectLTE() {
    // Connect with a maximum timeout value of 30 000 ms, if the connection is
    // not up and running within 30 seconds, abort and retry later
    if (!Lte.begin(30000)) {
        return false;
    } else {
        return true;
    }
}

static bool connectMqtt() {

    // Attempt to connect to the broker
    if (!MqttClient.beginAWS()) {
        return false;
    }

    return true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info(F("Starting MQTT with Connection Loss Handling\r\n"));

    if (initMQTTTopics() == false) {
        Log.error(F("Unable to initialize the MQTT topics. Stopping..."));
        while (1) {}
    }

    Lte.onDisconnect(disconnectedFromNetwork);

    MqttClient.onDisconnect(disconnectedFromBroker);
}

/**
 * @brief Used to keep track of the number of publishes.
 */
static uint32_t counter = 0;

/**
 * @brief Counts the times the publish has failed due to a timeout.
 */
static uint32_t failed_publishes = 0;

/**
 * @brief Used for keeping track of time so that a message is published every
 * 10th second under normal operations (when connected to the network and
 * broker).
 */
static uint32_t timer = 0;

void loop() {

    if (!connecteded_to_network) {
        Log.info(F("Not connected to the network. Attempting to connect!"));
        if (connectLTE()) {
            connecteded_to_network = true;
        }
    }

    if (!connected_to_broker && connecteded_to_network) {
        Log.info(F("Not connected to broker. Attempting to connect!"));

        if (connectMqtt()) {
            connected_to_broker = true;
        }
    }

    if (millis() - timer > 10000) {
        if (connected_to_broker) {
            char message_to_publish[8] = {0};
            sprintf(message_to_publish, "%lu", counter);

            bool published_successfully = MqttClient.publish(mqtt_pub_topic,
                                                             message_to_publish,
                                                             AT_LEAST_ONCE,
                                                             60000);
            if (published_successfully) {
                Log.infof(F("Published message: %s. Failed publishes: %d.\r\n"),
                          message_to_publish,
                          failed_publishes);
            } else {
                failed_publishes++;
            }

            counter++;
        }

        timer = millis();
    }
}

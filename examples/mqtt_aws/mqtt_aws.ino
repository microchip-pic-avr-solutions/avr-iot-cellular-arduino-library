/**
 * This example connects to the device specific endpoint in AWS in order to
 * publish and retrieve MQTT messages.
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
#define MQTT_SUB_TOPIC_FMT "%s/sensors"
#define MQTT_PUB_TOPIC_FMT "%s/sensors"

char mqtt_sub_topic[128];
char mqtt_pub_topic[128];

bool initTopics() {
    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf("Failed to initialize ECC, error code: %X\r\n", status);
        return false;
    }

    // Find the thing ID and set the publish and subscription topics
    uint8_t thing_name[128];
    size_t thing_name_length = sizeof(thing_name);

    status =
        ECC608.readProvisionItem(AWS_THINGNAME, thing_name, &thing_name_length);

    if (status != ATCA_SUCCESS) {
        Log.errorf(
            "Could not retrieve thingname from the ECC, error code: %X\r\n",
            status);
        return false;
    }

    snprintf(mqtt_sub_topic,
             sizeof(mqtt_sub_topic),
             MQTT_SUB_TOPIC_FMT,
             thing_name);
    snprintf(mqtt_pub_topic,
             sizeof(mqtt_pub_topic),
             MQTT_PUB_TOPIC_FMT,
             thing_name);

    return true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting MQTT for AWS example\r\n");

    if (!initTopics()) {
        Log.error("Unable to initialize the MQTT topics. Stopping...");
        while (1) {}
    }

    if (!Lte.begin()) {
        Log.error("Failed to connect to operator");
        while (1) {}
    }

    // Attempt to connect to AWS
    if (MqttClient.beginAWS()) {

        Log.infof("Connecting to AWS");

        while (!MqttClient.isConnected()) {
            Log.rawf(".");
            delay(500);
        }

        Log.rawf(" OK!\r\n");

        MqttClient.subscribe(mqtt_sub_topic);

    } else {
        Log.rawf("\r\n");
        Log.error("Failed to connect to AWS");
        while (1) {}
    }

    // Test MQTT publish and receive
    for (uint8_t i = 0; i < 3; i++) {

        const bool published_successfully =
            MqttClient.publish(mqtt_pub_topic, "{\"light\": 9, \"temp\": 9}");

        if (published_successfully) {
            Log.info("Published message");
        } else {
            Log.error("Failed to publish\r\n");
        }

        delay(2000);

        String message = MqttClient.readMessage(mqtt_sub_topic);

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a
        // new message
        if (message != "") {
            Log.infof("Got new message: %s\r\n", message.c_str());
        }

        delay(2000);
    }

    Log.info("Closing MQTT connection");

    MqttClient.end();
}

void loop() {}

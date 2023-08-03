/**
 * @brief This example demonstrates how to connect to the Azure IoT Hub using
 * the ATECC608 cryptographic chip on the AVR-Iot Cellular mini. Please make
 * sure your board is provisioned first with the provision sketch.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>

#define MQTT_PUB_TOPIC_FMT "devices/%s/messages/events/"
#define MQTT_SUB_TOPIC_FMT "%s/messages/devicebound/#"

static char mqtt_sub_topic[128];
static char mqtt_pub_topic[128];

bool initTopics() {
    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf("Failed to initialize ECC, error code: %X\r\n", status);
        return false;
    }

    // Find the device ID and set the publish and subscription topics
    uint8_t device_id[128];
    size_t device_id_length = sizeof(device_id);

    status =
        ECC608.readProvisionItem(AZURE_DEVICE_ID, device_id, &device_id_length);

    if (status != ATCA_SUCCESS) {
        Log.errorf(
            "Could not retrieve device ID from the ECC, error code: %X\r\n",
            status);
        return false;
    }

    snprintf(mqtt_sub_topic,
             sizeof(mqtt_sub_topic),
             MQTT_SUB_TOPIC_FMT,
             device_id);
    snprintf(mqtt_pub_topic,
             sizeof(mqtt_pub_topic),
             MQTT_PUB_TOPIC_FMT,
             device_id);

    return true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting MQTT for Azure example\r\n");

    if (!initTopics()) {
        Log.error("Unable to initialize the MQTT topics. Stopping...");
        while (1) {}
    }

    if (!Lte.begin()) {
        Log.error("Failed to connect to operator");
        while (1) {}
    }

    // Attempt to connect to Azure
    if (MqttClient.beginAzure()) {
        Log.infof("Connecting to Azure IoT Hub");

        while (!MqttClient.isConnected()) {
            Log.rawf(".");
            delay(500);
        }

        Log.rawf(" OK!\r\n");

        MqttClient.subscribe(mqtt_sub_topic);

    } else {
        Log.rawf("\r\n");
        Log.error("Failed to connect to Azure IoT Hub");
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
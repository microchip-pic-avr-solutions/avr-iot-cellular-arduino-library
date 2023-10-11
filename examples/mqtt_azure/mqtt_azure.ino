/**
 * @brief This example demonstrates how to connect to the Azure IoT Hub using
 * the ATECC608 cryptographic chip on the AVR-Iot Cellular mini. Please make
 * sure your board is provisioned first with the provision sketch.
 *
 * With Azure, we use a wildcard for subscription, so we need to enable a
 * callback for the received messages so that we can grab the specific topic.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>

const char MQTT_PUB_TOPIC_FMT[] PROGMEM = "devices/%s/messages/events/";
const char MQTT_SUB_TOPIC_FMT[] PROGMEM = "devices/%s/messages/devicebound/#";

static char mqtt_sub_topic[128];
static char mqtt_pub_topic[128];

static volatile bool got_message_event = false;
static char message_topic[384];
static volatile uint16_t message_length = 0;

static void onReceive(const char* topic,
                      const uint16_t length,
                      __attribute__((unused)) const int32_t id) {
    strcpy(message_topic, topic);
    message_length = length;

    got_message_event = true;
}

bool initTopics() {
    ATCA_STATUS status = ECC608.begin();

    if (status != ATCA_SUCCESS) {
        Log.errorf(F("Failed to initialize ECC, error code: %X\r\n"), status);
        return false;
    }

    // Find the device ID and set the publish and subscription topics
    uint8_t device_id[128];
    size_t device_id_length = sizeof(device_id);

    status =
        ECC608.readProvisionItem(AZURE_DEVICE_ID, device_id, &device_id_length);

    if (status != ATCA_SUCCESS) {
        Log.errorf(F("Could not retrieve device ID from the ECC, error code: "
                     "%X. Please provision the device with the provision "
                     "example sketch.\r\n"),
                   status);
        return false;
    }

    sprintf_P(mqtt_sub_topic, MQTT_SUB_TOPIC_FMT, device_id);
    sprintf_P(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, device_id);

    return true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info(F("Starting MQTT for Azure example\r\n"));

    if (!initTopics()) {
        Log.error(F("Unable to initialize the MQTT topics. Stopping..."));
        while (1) {}
    }

    if (!Lte.begin()) {
        Log.error(F("Failed to connect to operator"));
        while (1) {}
    }

    // Attempt to connect to Azure
    if (MqttClient.beginAzure()) {
        MqttClient.subscribe(mqtt_sub_topic);
        MqttClient.onReceive(onReceive);
    } else {
        while (1) {}
    }

    // Test MQTT publish and receive
    for (uint8_t i = 0; i < 3; i++) {

        const bool published_successfully =
            MqttClient.publish(mqtt_pub_topic, "{\"light\": 9, \"temp\": 9}");

        if (published_successfully) {
            Log.info(F("Published message"));
        } else {
            Log.error(F("Failed to publish\r\n"));
        }

        if (got_message_event) {

            String message = MqttClient.readMessage(message_topic,
                                                    message_length);

            // Read message will return an empty string if there were no new
            // messages, so anything other than that means that there were a
            // new message
            if (message != "") {
                Log.infof(F("Got new message: %s\r\n"), message.c_str());
            }

            got_message_event = false;
        }

        delay(2000);
    }

    Log.info(F("Closing MQTT connection"));

    MqttClient.end();
}

void loop() {}

/**
 * @brief This example demonstrates how to use MQTT with low power.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <mcp9808.h>
#include <mqtt_client.h>
#include <veml3328.h>

#define USE_PSM false

static char mqtt_pub_topic[128];

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

    sprintf_P(mqtt_pub_topic, PSTR("%s/sensors"), thingName);

    return true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info(F("Starting MQTT with low power"));

    // First we retrieve the topics we're going to publish to, here using the
    // ECC thingname with AWS
    if (initMQTTTopics() == false) {
        Log.error(F("Unable to initialize the MQTT topics. Stopping..."));
        while (1) {}
    }

    // Configure low power depending on whether to use power save mode or just a
    // complete power down.
#if USE_PSM
    LowPower.configurePeriodicPowerSave(
        PowerSaveModePeriodMultiplier::ONE_MINUTE,
        1);
#else
    LowPower.configurePowerDown();
#endif

    Lte.begin();

    // If we're using PSM, we only need to connect to the MQTT broker at the
    // beginning, since the connection will remain active
#if USE_PSM
    MqttClient.beginAWS();
#endif

    Mcp9808.begin();
    Veml3328.begin();
}

static uint32_t counter = 0;

void loop() {

    // If we're not using PSM, all connections will be terminated when power
    // down is issued, so we need to re-establish s connection
#if !USE_PSM
    MqttClient.beginAWS();
#endif

    char message_to_publish[8] = {0};
    sprintf(message_to_publish, "%lu", counter);

    bool published_successfully =
        MqttClient.publish(mqtt_pub_topic, message_to_publish, AT_LEAST_ONCE);

    if (published_successfully) {
        Log.infof(F("Published message: %s.\r\n"), message_to_publish);
    } else {
        Log.info(F("Failed to publish"));
    }

    counter++;

    Mcp9808.shutdown();
    Veml3328.shutdown();

    Log.info(F("Entering low power"));
    delay(10);

#if USE_PSM
    LowPower.powerSave();
#else
    LowPower.powerDown(60);
#endif

    Log.info(F("Woke up!"));

    Mcp9808.wake();
    Veml3328.wake();

    delay(10000);
}

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

#define MQTT_THING_NAME "someuniquemchp"
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_PUB_TOPIC  "testtopic"
#define MQTT_PORT       1883
#define MQTT_USE_TLS    false
#define MQTT_USE_ECC    false
#define MQTT_KEEPALIVE  180

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info(F("Starting MQTT with low power"));

    // Configure low power depending on whether to use power save mode or just a
    // complete power down.
#if USE_PSM
    LowPower.configurePeriodicPowerSave(
        PowerSaveModePeriodMultiplier::ONE_MINUTE,
        3);
#else
    LowPower.configurePowerDown();
#endif

    Lte.begin();

    // If we're using PSM, we only need to connect to the MQTT broker at the
    // beginning, since the connection will remain active
    //
    // Here we also set the keep alive to 3 minutes to match the sleep period
    // for PSM
#if USE_PSM
    MqttClient.begin(MQTT_THING_NAME,
                     MQTT_BROKER,
                     MQTT_PORT,
                     MQTT_USE_TLS,
                     MQTT_KEEPALIVE,
                     MQTT_USE_ECC);
#endif

    Mcp9808.begin();
    Veml3328.begin();
}

static uint32_t counter = 0;

void loop() {

    // If we're not using PSM, all connections will be terminated when power
    // down is issued, so we need to re-establish s connection
#if !USE_PSM
    MqttClient.begin(MQTT_THING_NAME,
                     MQTT_BROKER,
                     MQTT_PORT,
                     MQTT_USE_TLS,
                     MQTT_KEEPALIVE,
                     MQTT_USE_ECC);
#endif

    char message_to_publish[8] = {0};
    sprintf(message_to_publish, "%lu", counter);

    bool published_successfully =
        MqttClient.publish(MQTT_PUB_TOPIC, message_to_publish, AT_LEAST_ONCE);

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

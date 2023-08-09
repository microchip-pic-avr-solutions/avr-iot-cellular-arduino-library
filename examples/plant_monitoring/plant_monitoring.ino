/**
 * @brief Demonstrates a plant monitoring system that sends data to AWS. For
 * more information, head to the examples in the online AVR-IoT Cellular Mini
 * documentation.
 */

#include <Adafruit_AHTX0.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_seesaw.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include <mcp9808.h>
#include <veml3328.h>

#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <mqtt_client.h>

#define DEVICE_ID     "device_id"
#define AWS_PUB_TOPIC "sensorData"

// Define whether you want to use power save (not available on all operators),
// power down (shuts down the modem completely) or no low power mode at all
// (both commented out)
// #define USE_POWER_SAVE
// #define USE_POWER_DOWN

Adafruit_seesaw seesaw(&Wire1);
Adafruit_AHTX0 aht     = Adafruit_AHTX0();
Adafruit_VEML7700 veml = Adafruit_VEML7700();

/**
 * @brief  Initializes the sensors used by the plant monitoring example. Will
 * also shutdown the onboard sensors to save power.
 *
 * @return true If all sensors were successfully initialized.
 */
bool setupSensors() {
    if (!seesaw.begin(0x36)) {
        Log.error("Adafruit seesaw not found.");
        return false;
    }

    if (!aht.begin(&Wire1)) {
        Log.error("Adafruit AHT not found.");
        return false;
    }

    if (!veml.begin(&Wire1)) {
        Log.error("Adafruit VEML7700 not found.");
        return false;
    }

    // We want to shutdown the onboard sensors to save power as they are not
    // used
    if (Mcp9808.begin()) {
        Log.error("Could not start MCP9808.");
        return false;
    }

    Mcp9808.shutdown();

    if (Veml3328.begin()) {
        Log.error("Could not start VEML3328.");
        return false;
    }

    Veml3328.shutdown();

    return true;
}

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.begin(115200);
    Log.info("Starting up plant monitoring example\r\n");

    if (!setupSensors()) {
        while (1) {}
    }

#if defined(USE_POWER_SAVE) && !defined(USE_POWER_DOWN)
    // Configure power save. Here we set to sleep for 30 minutes
    LowPower.configurePeriodicPowerSave(PowerSaveModePeriodMultiplier::MINUTES,
                                        1);
#elif !defined(USE_POWER_SAVE) && defined(USE_POWER_DOWN)
    // Configure power down. Note that here we don't need to preconfigure the
    // time to power down
    LowPower.configurePowerDown();
#elif defined(USE_POWER_SAVE) && defined(USE_POWER_DOWN)
#error "Cannot use both power save and power down at the same time"
#endif
}

/**
 * @brief Collects data, JSON encodes it and places it in the
 * @p data buffer.
 *
 * @param data [out] Buffer to place the JSON encoded data in.
 * @param data_capacity [in] The capacity of the data buffer.
 *
 * @return The number of bytes written to the @p data buffer.
 */
size_t retrieveData(char* data, const size_t data_capacity) {
    // Get air metrics
    StaticJsonDocument<50> air_data;
    sensors_event_t humidity, temperature;
    aht.getEvent(&humidity, &temperature);

    air_data["Temperature"]  = temperature.temperature;
    air_data["Humidity"]     = humidity.relative_humidity;
    air_data["Illumination"] = veml.readLux();

    // Get soil metrics
    StaticJsonDocument<25> soil_data;
    const float moisture_level = 100 * ((float)seesaw.touchRead(0) / 1023);
    soil_data["Moisture"]      = moisture_level;

    // Get device metrics
    StaticJsonDocument<25> device_data;
    device_data["SupplyVoltage"] = LowPower.getSupplyVoltage();

    // Build data payload
    StaticJsonDocument<200> payload;
    payload["Device_ID"] = DEVICE_ID;
    payload["Air"]       = air_data;
    payload["Soil"]      = soil_data;
    payload["Board"]     = device_data;

    return serializeJson(payload, data, data_capacity);
}

void loop() {

    // Check first if we are connected to the operator. We might get
    // disconnected, so for our application to continue to run, we double check
    // here
    if (!Lte.isConnected()) {

        // Attempt to connect to the operator
        if (!Lte.begin()) {
            return;
        }

        // Attempt to connect to AWS
        if (MqttClient.beginAWS()) {

            Log.infof("Connecting to AWS");

            while (!MqttClient.isConnected()) {
                Log.rawf(".");
                delay(500);
            }

            Log.rawf(" OK!\r\n");
        } else {
            Log.error("Failed to configure MqttClient for AWS");
            while (1) {}
        }
    }

    // Data is declared static here so that it appears in RAM
    // usage during compilation. In that way, we have a better
    // estimate of how much RAM is used.
    static char data[512] = "";

    if (retrieveData(data, sizeof(data)) > sizeof(data)) {
        Log.error("Data buffer too small.");
        while (1) {}
    }

    /*
    Log.infof("Publishing data: %s\r\n", data);
    if (!MqttClient.publish(AWS_PUB_TOPIC, data)) {
        Log.warn("Failed to publish data");
    }
    */

#if defined(USE_POWER_SAVE) && !defined(USE_POWER_DOWN)
    LowPower.powerSave();
#elif !defined(USE_POWER_SAVE) && defined(USE_POWER_DOWN)
    // Power down for 1 minute
    LowPower.powerDown(60);
#elif defined(USE_POWER_SAVE) && defined(USE_POWER_DOWN)
#error "Cannot use both power save and power down at the same time"
#endif

    delay(5000);
}

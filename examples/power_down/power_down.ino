/**
 * Note that this example requires the Mcp9808 and Veml3328 driver, they can be
 * found in the Arduino IDE's library manager.
 */

#include <avr/io.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <mcp9808.h>
#include <veml3328.h>

void setup() {
    Log.begin(115200);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Now we configure the low power module for power down configuration, where
    // the cellular modem and the CPU will be powered down
    LowPower.configurePowerDown();

    // Make sure sensors are turned off
    Veml3328.begin();
    Mcp9808.begin();
    Veml3328.shutdown();
    Mcp9808.shutdown();

    // Connecting to the network might fail, so we just retry
    while (!Lte.begin()) {}

    Log.infof("Connected to operator: %s\r\n", Lte.getOperator().c_str());
}

void loop() {
    Log.info("Powering down...");

    // Allow some time for the log message to be transmitted before we power
    // down
    delay(100);

    // Power down for 60 seconds
    LowPower.powerDown(60);

    Log.info("Woke up!");

    // Do work ...
    Log.info("Doing work...");
    delay(10000);
}

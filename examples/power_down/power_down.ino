#include <avr/io.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <mcp9808.h>
#include <veml3328.h>

#define SW0 PIN_PD2

ISR(PORTD_PORT_vect) {
    if (PORTD.INTFLAGS & PIN2_bm) {
        // Reset the interupt flag so that we can process the next incoming
        // interrupt
        PORTD.INTFLAGS = PIN2_bm;
    }
}

void setup() {
    Log.begin(115200);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Configure SW0 for interrupt so we can wake the device up from sleep by
    // pressing the button
    pinConfigure(SW0, PIN_DIR_INPUT | PIN_INT_FALL);

    // Now we configure the low power module for power down configuration, where
    // the LTE modem and the CPU will be powered down
    LowPower.configurePowerDown();

    // Make sure sensors are turned off
    Veml3328.begin();
    Mcp9808.begin();
    Veml3328.shutdown();
    Mcp9808.shutdown();

    if (!Lte.begin()) {
        Log.error("Failed to connect to operator");

        // Halt here
        while (1) {}
    }

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

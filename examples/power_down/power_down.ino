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

#define SW0 PIN_PD2

static volatile bool woke_up_from_button = false;

void button_pressed_callback(void) {
    if (PORTD.INTFLAGS & PIN2_bm) {
        // Reset the interrupt flag so that we can process the next incoming
        // interrupt
        PORTD.INTFLAGS = PIN2_bm;

        woke_up_from_button = true;
    }
}

void setup() {
    Log.begin(115200);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Attach interrupt callback to detect if SW0 was pressed during sleep
    pinConfigure(SW0, PIN_DIR_INPUT | PIN_PULLUP_ON);
    attachInterrupt(SW0, button_pressed_callback, FALLING);

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

    Log.infof(F("Connected to operator: %s\r\n"), Lte.getOperator().c_str());
}

void loop() {
    Log.info(F("Powering down..."));

    // Allow some time for the log message to be transmitted before we power
    // down
    delay(100);

    // Power down for 60 seconds
    LowPower.powerDown(60);

    if (woke_up_from_button) {
        Log.info(F("SW0 was pressed"));
        woke_up_from_button = false;
    }

    Log.info(F("Woke up!"));

    // Do work ...
    Log.info(F("Doing work..."));
    delay(10000);
}

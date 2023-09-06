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
#include <sequans_controller.h>
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

    // Now we configure the power save configuration. Note that this has to be
    // done before calling Lte.begin().
    //
    // This mode for low power is somewhat more complex than a plain power down,
    // which cuts the power to the cellular modem and puts the AVR CPU in sleep
    // for as long as we want.
    //
    // Let's break this mode down:
    //
    // 1. We first configure the cellular modem with this power save
    // configuration. That tells the cellular modem that it will be active for
    // some time period sending data and syncing up with the operator, then it
    // can go to sleep for the remaining time of this time period. That means
    // that the cellular modem will not be sleeping for the whole time period,
    // but most of it. This repeats as long as we want.
    //
    // Here we say that we want to have a power save period of 1 minute * 3 =
    // 180 seconds.
    //
    // 2. This happens periodically as long as we tell it that it's okay for it
    // to go into power save, which we do with Power.powerSave(). Note that
    // the cellular modem is the one responsible for knowing where we are in
    // this time period, thus, if we call Power.powerSave() in the middle of the
    // time period, it will only sleep for half the time before being woken up
    // again. This is totally fine, and can for example happen if we do some
    // operations on the CPU which takes a lot of time.
    //
    // In powerSave(), after the cellular modem is sleeping, we also put the
    // CPU to sleep. When the time period is over, the CPU is woken at the same
    // time as the cellular modem is woken up.

    LowPower.configurePeriodicPowerSave(
        PowerSaveModePeriodMultiplier::ONE_MINUTE,
        3);

    // Make sure sensors are turned off
    Veml3328.begin();
    Mcp9808.begin();
    Veml3328.shutdown();
    Mcp9808.shutdown();

    // Connecting to the network might fail, so we just retry
    if (!Lte.begin()) {
        Log.error(F("Failed to connect to operator"));

        // Halt here
        while (true) {}
    }

    Log.infof(F("Connected to operator: %s\r\n"), Lte.getOperator().c_str());
}

void loop() {
    Log.info(F("Power saving..."));
    delay(100);

    LowPower.powerSave();

    if (woke_up_from_button) {
        Log.info(F("SW0 was pressed"));
        woke_up_from_button = false;
    }

    Log.info(F("Woke up!"));

    // Do work ...
    Log.info(F("Doing work..."));
    delay(10000);
}

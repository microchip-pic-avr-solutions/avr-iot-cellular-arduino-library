#include <avr/cpufunc.h>
#include <avr/io.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <sequans_controller.h>

#ifdef __AVR_AVR128DB48__ // MINI

#define SerialDebug Serial3

#else
#ifdef __AVR_AVR128DB64__ // Non-Mini

#define SerialDebug Serial5

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

#define SW0 PIN_PD2

ISR(PORTD_PORT_vect) {
    if (PORTD.INTFLAGS & PIN2_bm) {
        PORTD.INTFLAGS = PIN2_bm;
    }
}

void setup() {
    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Configure SW0 for interrupt so we can wake the device up from sleep by
    // pressing the button
    pinConfigure(SW0, PIN_DIR_INPUT | PIN_INT_FALL);

    // Now we configure the power save configuration. Note that this has to be
    // done before calling Lte.begin().
    //
    // This mode for LowPower is somewhat more complex than a plain power down,
    // which cuts the power to the LTE modem and puts the AVR CPU in sleep for
    // as long as we want.
    //
    // Let's break this mode down:
    //
    // 1. We first configure the LTE modem with this power save configuration.
    // That tells the LTE modem that it will be active for some time period
    // sending data and syncing up with the operator, then it can go to sleep
    // for the remaining time of this time period. That means that the LTE modem
    // will not be sleeping for the whole time period, but most of it. This
    // repeats as long as we want.
    //
    // Here we say that we want to have a power save period of 30 seconds * 2 =
    // 60 seconds.
    //
    // 2. This happens periodically as long as we tell it that it's okay for it
    // to go into power save, which we do with LowPower.powerSave(). Note that
    // the LTE modem is the one responsible for knowing where we are in this
    // time period, thus, if we call LowPower.powerSave() in the middle of the
    // time period, it will only sleep for half the time before being woken up
    // again. This is totally fine, and can for example happen if we do some
    // operations on the CPU which takes a lot of time.
    //
    // In powerSave(), after the LTE modem is sleeping, we also put the
    // CPU to sleep. When the time period is over, the CPU is woken at the same
    // time as the LTE modem is woken up.

    LowPower.configurePeriodicPowerSave(
        PowerSaveModePeriodMultiplier::THIRTY_SECONDS, 2);

    Lte.begin();
    Log.infof("Connecting to operator");

    while (!Lte.isConnected()) {
        Log.raw(".");
        delay(1000);
    }

    Log.raw("\r\n");
    Log.infof("Connected to operator: %s\r\n", Lte.getOperator().c_str());
}

void loop() {
    Log.raw("\r\n");
    Log.info("Going to sleep...");
    delay(100);

    LowPower.powerSave();

    // Do work ...
    Log.info("Doing work...");
    delay(10000);
}

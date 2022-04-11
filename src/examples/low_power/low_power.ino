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

    // Configure the power save configuration, start the LTE modem and wait
    // until we are connected to the operator
    //
    // Here we say that we want to sleep for 30 seconds * 2 = 60 seconds each
    // time we invoke sleep
    LowPower.begin(SleepMultiplier::THIRTY_SECONDS, 2, SleepMode::REGULAR);
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
    WakeUpReason wakeup_reason = LowPower.sleep();

    switch (wakeup_reason) {
    case WakeUpReason::OK:
        Log.info("Finished sleep");
        break;
    case WakeUpReason::EXTERNAL_INTERRUPT:
        Log.info("Got woken up by external interrupt");
        break;
    case WakeUpReason::AWOKEN_BY_MODEM_PREMATURELY:
        Log.info("Got woken up by modem prematurely");
        break;
    case WakeUpReason::MODEM_TIMEOUT:
        Log.info("Took too long to put modem in sleep, no time left for "
                 "sleeping. You might have to increase the sleep time.");
        break;
    case WakeUpReason::INVALID_SLEEP_TIME:
        Log.info("Got invalid sleep time from operator");
        break;
    }

    // Do work ...
    Log.info("Doing work...");
    delay(10000);
}

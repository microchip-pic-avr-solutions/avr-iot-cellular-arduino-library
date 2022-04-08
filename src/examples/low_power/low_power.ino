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

void setup() {

    Log.begin(115200);

    LedCtrl.begin();
    LedCtrl.startupCycle();

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
    Log.infof("Got out of sleep with wake up reason %d, doing work...\r\n",
              wakeup_reason);

    delay(10000);
    // Do work ...
}

#include <avr/cpufunc.h>
#include <avr/io.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>

void setup() {

    LedCtrl.begin();

    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    // Configure the power save configuration, start the LTE modem and wait
    // until we are connected to the operator
    //
    // Here we say that we want to sleep for 30 seconds * 2 = 60 seconds each
    // time we invoke sleep
    LowPower.begin(SleepMultiplier::THIRTY_SECONDS, 2, SleepMode::DEEP);
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Waiting for connection...\r\n");
        delay(2000);
    }

    Log.info("Connected to operator!\r\n");
}

void loop() {

    WakeUpReason wakeup_reason = LowPower.sleep();
    Log.infof("Got out of sleep with wake up reason %d, doing work...\r\n",
              wakeup_reason);

    delay(10000);
    // Do work ...
}

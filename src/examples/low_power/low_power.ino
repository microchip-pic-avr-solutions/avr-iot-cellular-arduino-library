#include <Arduino.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <sequans_controller.h>

#ifdef __AVR_AVR128DB48__ // MINI

#define SerialDebug Serial3
#define TIMING_PIN  PIN_PE2
#else
#ifdef __AVR_AVR128DB64__ // Non-Mini

#define SerialDebug Serial5
#define TIMING_PIN  PIN_PE7

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

void setup() {

    LedCtrl.begin();

    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    // Start LTE modem, configure the power save configuration and wait until we
    // are connected to the operator
    //
    // Here we say that we want to sleep for 30 seconds * 2 = 60 seconds each
    // time we invoke sleep
    LowPower.begin(SleepMultiplier::THIRTY_SECONDS, 2, SleepMode::REGULAR);
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Waiting for connection...\r\n");
        delay(2000);
    }

    Log.info("Connected to operator!\r\n");

    pinConfigure(TIMING_PIN, PIN_DIR_OUTPUT);
}

void loop() {

    digitalWrite(TIMING_PIN, digitalRead(TIMING_PIN) ? 0 : 1);

    WakeUpReason wakeup_reason = LowPower.sleep();
    Log.infof("Got out of sleep with wake up reason %d, doing work...\r\n",
              wakeup_reason);
}

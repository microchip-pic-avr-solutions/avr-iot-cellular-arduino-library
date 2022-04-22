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

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Configure SW0 for interrupt so we can wake the device up from sleep by
    // pressing the button
    pinConfigure(SW0, PIN_DIR_INPUT | PIN_INT_FALL);

    // Now we configure the low power module for power down configuration.
    LowPower.configurePowerDown();

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
    Log.info("Powering down...");
    delay(100);

    // Power down for 60 seconds
    LowPower.powerDown(60);

    Log.info("Woke up!");

    // Do work ...
    Log.info("Doing work...");
    delay(10000);
}

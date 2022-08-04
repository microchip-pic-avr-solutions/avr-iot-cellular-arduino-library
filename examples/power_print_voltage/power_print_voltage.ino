/**
 * @brief This example demonstrates how to retrieve the current voltage supplied
 * to the board. When the board is plugged in with an USB cable, the default
 * voltage will be around 4.7V due to a voltage drop over a diode. When not
 * plugged in with an USB cable and a battery is used instead, the voltage
 * reported will be the battery voltage.
 *
 * Note that in order to print floats, you have to set printf support to full in
 * the Arduino IDE. Go to Tools -> printf() -> Full
 */

#include <led_ctrl.h>
#include <log.h>
#include <power.h>

void setup() {
    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting up example for printing voltage supplied to the board");
}

void loop() {

    float voltage = Power.getSupplyVoltage();

    // Note that in order to print floats, you have to set printf support to
    // full in the Arduino IDE. Go to Tools -> printf() -> Full
    Log.infof("The voltage supplied is: %f\r\n", voltage);

    delay(1000);
}

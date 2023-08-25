/**
 * @brief This example demonstrates how to use the GPIO pins on the board, where
 * for the AVR-IoT Cellular board the pin naming is a bit different than in a
 * regular arduino environment.
 */
#include <Arduino.h>

/**
 * @brief Note that it is important that variables modified in an interrupt are
 * volatile, so that the compiler don't optimize them away.
 */
static volatile bool button_pressed = false;

/**
 * @brief Gets called when the SW0 button is pressed.
 */
static void buttonInterrupt(void) { button_pressed = true; }

void setup() {
    // The pins on the board are not named in a typical way as with standard
    // Arduino (with numbers), but with port and pin. The pinout can be seen in
    // the hardware user guide located at (figure 2-1, page 7):
    // https://ww1.microchip.com/downloads/en/DeviceDoc/AVR-IoT-Cellular-Mini-HW-UserGuide-DS50003320.pdf
    //
    // In order to modify GPIO pins, we need to use the light gray values showed
    // in the hardware user guide (e.g. PB2, which stands for port B and pin 2)

    // To configure a pin as output (in this case PB2, which is the user led),
    // do the following:
    pinConfigure(PIN_PB2, PIN_DIR_OUTPUT);

    // Then we can turn on the led by a regular digitalWrite (note that the LED
    // is active low)
    digitalWrite(PIN_PB2, LOW);

    // In order to configure a pin as an input, for example the button SW0 at
    // PD2, we can do the following:
    pinConfigure(PIN_PD2, PIN_DIR_INPUT | PIN_PULLUP_ON);

    // Then we can e.g. attach an interrupt to the button when it is pressed (on
    // the falling edge). If we want to have an interrupt when the button is
    // released, we'd need to use the rising edge instead.
    //
    // We could also use digitalRead(PIN_PD2) in a loop in order to check the
    // value continuously
    attachInterrupt(PIN_PD2, buttonInterrupt, FALLING);

    // We start the Serial3, which is used to print messages. As one can also
    // see on the same page in the hardware user guide, the USART3 is used for
    // sending messages to the debugger, which again is connected to the
    // computer via USB. We thus have to be careful to use Serial3 and not
    // Serial for printing
    Serial3.begin(115200);

    // Analog functionality is quite similar, we can for example set up analog
    // reading of the voltage measurement pin on the board. As one can see in
    // the hardware user guide, this pin is PE0 (under power supply
    // connections).

    // First we need to set the voltage measure pin (PB3) high to tell the
    // hardware that we want to read the supply voltage:
    pinConfigure(PIN_PB3, PIN_DIR_OUTPUT);
    digitalWrite(PIN_PB3, HIGH);

    // Delay some to let the changes take effect
    delay(100);

    // Now we can do a regular analog read. We do one analogRead first as the
    // result initially might be unstable and not settled.
    analogRead(PIN_PE0);

    float analog_value = (float)analogRead(PIN_PE0);

    // The analog value will be a value from 0 to 1023 (10 bit resolution), so
    // we divide by 1023 to get a value between 0 and 1:
    analog_value /= 1023.0f;

    // The input pin runs on a logic level of 3.3 V, so we have to upscale by
    // that
    analog_value *= 3.3f;

    // The voltage read is actually 1/4 of the true value since it is within a
    // voltage divider, so we have to multiply by 4 (look at page 32 in the
    // hardware user guide under VMUX Voltage Measure):
    analog_value *= 4.0f;

    // You should see a voltage of approximately 4.8 V for when the board is
    // connected through USB (USB is delivering 5 V, but there are some
    // voltage drop over the components on the way to the voltage measurement
    // circuit)
    Serial3.print(F("The voltage supplied is: "));
    Serial3.println(analog_value);
}

void loop() {

    if (button_pressed) {
        Serial3.println(F("Button pressed"));
        button_pressed = false;
    }
}
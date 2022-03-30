#ifndef LED_CTRL_H
#define LED_CTRL_H

#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

enum class Led { CELL = 0, CON, DATA, ERROR, USER };

class LedCtrlClass {

  private:
    /**
     * @brief Hide constructor in order to enforce a single instance of the
     * class.
     */
    LedCtrlClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static LedCtrlClass &instance(void) {
        static LedCtrlClass instance;
        return instance;
    }

    /**
     * @brief Starts the LED control module.
     *
     * @param manual_control Set to true if the system should *not* modify the
     * leds automatically (for example when the LTE modem is connected).
     */
    void begin();

    /**
     * @brief Start the LED control module in manual mode. In manual mode, the
     * system should *not* modify the LEDs automatically (for example when the
     * LTE modem is connected).
     *
     */
    void beginManual();

    /**
     * @return The pin associated with the given LED.
     */
    uint8_t getLedPin(Led led);

    /**
     * @brief Toggle @p led.
     *
     * @param is_from_system_event If set to true, will toggle the led only if
     * manual control is disabled.
     */
    void toggle(Led led, bool is_from_system_event = false);

    /**
     * @brief Set @p led on.
     *
     * @param is_from_system_event If set to true, will turn the led on only if
     * manual control is disabled.
     */
    void on(Led led, bool is_from_system_event = false);

    /**
     * @brief Set @p led off.
     *
     * @param is_from_system_event If set to true, will turn the led off only if
     * manual control is disabled.
     */
    void off(Led led, bool is_from_system_event = false);

    /**
     * @brief Cycles each LED for testing purposes
     */
    void startupCycle();
};

extern LedCtrlClass LedCtrl;

#endif

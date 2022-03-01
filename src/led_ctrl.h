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

    uint8_t getLedPin(Led led);

  public:
    /**
     * @brief Singleton instance.
     */
    static LedCtrlClass &instance(void) {
        static LedCtrlClass instance;
        return instance;
    }

    bool begin(const bool manualControl = false);

    void toggle(Led led, bool isSystem = false);
    void on(Led led, bool isSystem = false);
    void off(Led led, bool isSystem = false);
    void startupCycle();
};

extern LedCtrlClass LedCtrl;

#endif

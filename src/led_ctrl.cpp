#include "led_ctrl.h"
#include "log.h"

#ifdef __AVR_AVR128DB48__ // MINI

#define LED_CELL_PIN  PIN_PA0
#define LED_CON_PIN   PIN_PA1
#define LED_DATA_PIN  PIN_PA2
#define LED_ERROR_PIN PIN_PA3
#define LED_USER_PIN  PIN_PB2

#else
#ifdef __AVR_AVR128DB64__ // NON-MINI

#define LED_CELL_PIN  PIN_PG2
#define LED_CON_PIN   PIN_PG3
#define LED_DATA_PIN  PIN_PG4
#define LED_ERROR_PIN PIN_PG5
#define LED_USER_PIN  PIN_PB0

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

static bool manual_control_enabled = false;

void LedCtrlClass::begin() {
    pinConfigure(LED_CELL_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    pinConfigure(LED_CON_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    pinConfigure(LED_DATA_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    pinConfigure(LED_ERROR_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    pinConfigure(LED_USER_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);

    this->off(Led::CELL);
    this->off(Led::CON);
    this->off(Led::DATA);
    this->off(Led::ERROR);
    this->off(Led::USER);
}

void LedCtrlClass::beginManual() {
    manual_control_enabled = true;
    this->begin();
}

uint8_t LedCtrlClass::getLedPin(Led led) {
    uint8_t targetPin = 0;
    switch (led) {
    case Led::CELL:
        targetPin = LED_CELL_PIN;
        break;
    case Led::CON:
        targetPin = LED_CON_PIN;
        break;
    case Led::DATA:
        targetPin = LED_DATA_PIN;
        break;
    case Led::ERROR:
        targetPin = LED_ERROR_PIN;
        break;
    case Led::USER:
        targetPin = LED_USER_PIN;
        break;
    default:
        Log.errorf("attempted to get invalid led pin %d\r\n", led);
        return 200;
    }

    return targetPin;
}

void LedCtrlClass::on(Led led, bool is_from_system_event) {
    if (is_from_system_event && manual_control_enabled)
        return;

    digitalWrite(getLedPin(led), 0);
}

void LedCtrlClass::off(Led led, bool is_from_system_event) {
    if (is_from_system_event && manual_control_enabled)
        return;

    digitalWrite(getLedPin(led), 1);
}

void LedCtrlClass::toggle(Led led, bool is_from_system_event) {
    if (is_from_system_event && manual_control_enabled)
        return;

    const uint8_t led_pin = getLedPin(led);
    digitalWrite(led_pin, !digitalRead(led_pin));
}

void LedCtrlClass::startupCycle() {
    for (int i = int(Led::CELL); i <= int(Led::USER); i++) {
        this->on(Led(i));
        delay(50);
    }

    for (int i = int(Led::CELL); i <= int(Led::USER); i++) {
        this->off(Led(i));
        delay(50);
    }

    for (int i = int(Led::USER); i >= int(Led::CELL); i--) {
        this->on(Led(i));
        delay(50);
    }

    for (int i = int(Led::USER); i >= int(Led::CELL); i--) {
        this->off(Led(i));
        delay(50);
    }
}

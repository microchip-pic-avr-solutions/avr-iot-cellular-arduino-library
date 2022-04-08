#include "led_ctrl.h"
#include "log.h"
#include "low_power.h"
#include "lte.h"
#include "sequans_controller.h"

#include <Arduino.h>
#include <avr/cpufunc.h>
#include <avr/io.h>
#include <avr/sleep.h>

#define AT_COMMAND_DISABLE_EDRX       "AT+SQNEDRX=0"
#define AT_COMMAND_SET_PSM            "AT+CPSMS=1,,,\"%s\",\"%s\""
#define AT_COMMAND_SET_RING_BEHAVIOUR "AT+SQNRICFG=1,2,1000"
#define AT_COMMAND_CONNECTION_STATUS  "AT+CEREG?"

// For use of the rest of library, for example low power
#define RESPONSE_CONNECTION_STATUS_SIZE 70

// Command without arguments: 18 bytes
// Both arguments within the quotes are strings of 8 numbers: 8 * 2 = 16 bytes
// Total: 18 + 16 = 34 bytes
#define AT_COMMAND_SET_PSM_LENGTH 34

// Max is 0b11111 = 31 for the value of the timers for power saving mode (not
// the multipliers).
#define PSM_VALUE_MAX 31

// How long we wait between checking the ring line for activity
#define PSM_WAITING_TIME_DELTA_MS 50

// How long time we require the ring line to be stable before declaring that we
// have entered power save mode
#define PSM_RING_LINE_STABLE_THRESHOLD_MS 2500
#define PSM_MULTIPLIER_BM                 0xE0
#define PSM_VALUE_BM                      0x1F

// We set the active timer parameter to two seconds with this. The reason
// behind this is that we don't care about the active period since after
// sleep has finished, the RTS line will be active and prevent the modem
// from going to sleep and thus the awake time is as long as the RTS
// line is kept active.
//
// This parameter thus only specifies how quickly the modem can go to sleep,
// which we want to be as low as possible.
#define PSM_DEFAULT_AWAKE_PARAMETER "00000001"
#define PSM_DEFAULT_AWAKE_TIME      2

#define PSM_REMAINING_SLEEP_TIME_THRESHOLD 5

// This includes null termination
#define TIMER_LENGTH      11
#define TIMER_SLEEP_INDEX 8

#define RING_PORT VPORTC

#ifdef __AVR_AVR128DB48__ // MINI

#define RING_PIN_bm PIN6_bm

#else

#ifdef __AVR_AVR128DB64__ // Non-Mini

#define RING_PIN_bm PIN4_bm

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

// Singleton. Defined for use of the rest of the library.
LowPowerClass LowPower = LowPowerClass::instance();

static volatile bool ring_line_activity = false;
static volatile bool modem_is_in_power_save = false;
static volatile bool pit_triggered = false;

static SleepMode sleep_mode;
static bool retrieved_sleep_time = false;
static uint32_t sleep_time = 0;

static uint8_t cell_led_state = 0;
static uint8_t con_led_state = 0;

ISR(RTC_PIT_vect) {
    RTC.PITINTFLAGS = RTC_PI_bm;
    pit_triggered = true;
}

static void ring_line_callback(void) {

    ring_line_activity = true;

    if (modem_is_in_power_save) {
        modem_is_in_power_save = false;

        // We got abrupted, bring the power save mode off such that UART
        // communication is possible again
        SequansController.setPowerSaveMode(0, NULL);
    }
}

static void uint8ToStringOfBits(const uint8_t value, char *string) {
    // Terminate
    string[8] = 0;

    for (uint8_t i = 0; i < 8; i++) {
        string[i] = (value & (1 << (7 - i))) ? '1' : '0';
    }
}

static uint8_t stringOfBitsToUint8(const char *string) {

    uint8_t value = 0;

    for (uint8_t i = 0; i < 8; i++) {
        // We assume all other values are zero, so we only shift the ones
        if (string[i] == '1') {
            value |= (1 << (7 - i));
        }
    }

    return value;
}

static uint16_t decodeSleepMultiplier(const SleepMultiplier sleep_multiplier) {

    switch (sleep_multiplier) {
    case SleepMultiplier::TEN_HOURS:
        return 36000;
    case SleepMultiplier::ONE_HOUR:
        return 3600;
    case SleepMultiplier::TEN_MINUTES:
        return 600;
    case SleepMultiplier::ONE_MINUTE:
        return 60;
    case SleepMultiplier::THIRTY_SECONDS:
        return 30;
    case SleepMultiplier::TWO_SECONDS:
        return 2;
    default:
        return 0;
    }
}

/**
 * @brief Will attempt to put the LTE modem in power save mode.
 *
 * @note Will wait for @p waiting_time to see if the modem gets to low power
 * mode.
 *
 * @note The power save mode can be abrupted if a new message arrives from
 * the network (for example a MQTT message). Such messages have to be
 * handled before the LTE modem can be put back in into power save mode.
 *
 * @param waiting_time_ms How long to wait for the modem to get into low
 * power mode before giving up (in milliseconds).
 *
 * @return true if the LTE modem was put in power save mode.
 */
static bool
attemptToEnterPowerSaveModeForModem(const unsigned long waiting_time_ms) {

    // First we make sure the UART buffers are empty on the modem's side so the
    // modem can go to sleep
    SequansController.setPowerSaveMode(0, NULL);

    do {
        delay(50);
        SequansController.clearReceiveBuffer();
    } while (SequansController.isRxReady());

    SequansController.setPowerSaveMode(1, ring_line_callback);

    ring_line_activity = false;

    // Now we wait until the ring line to stabilize
    unsigned long last_time_active = millis();
    const unsigned long start_time = millis();

    do {
        delay(PSM_WAITING_TIME_DELTA_MS);

        // Reset timer if there has been activity or the RING line is high
        if (ring_line_activity || RING_PORT.IN & RING_PIN_bm) {
            last_time_active = millis();
            ring_line_activity = false;
        }

        if (millis() - last_time_active > PSM_RING_LINE_STABLE_THRESHOLD_MS) {
            modem_is_in_power_save = true;
            return true;
        }
    } while (millis() - start_time < waiting_time_ms);

    return false;
}

/**
 * @brief Finds the sleep time we got from the operator, which may deviate from
 * what we requested.
 *
 * @return The sleep time in seconds. 0 if error happened and the sleep time
 * couldn't be processed.
 */
static uint32_t retrieveOperatorSleepTime(void) {

    // First we call CEREG in order to get the byte where the sleep time is
    // encoded
    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE] = "";
    const ResponseResult response_result = SequansController.readResponse(
        response, RESPONSE_CONNECTION_STATUS_SIZE);

    if (response_result != ResponseResult::OK) {
        char response_result_string[18] = "";
        SequansController.responseResultToString(response_result,
                                                 response_result_string);
        Log.warnf("Did not get response result OK when retriving operator "
                  "sleep time: %d, %s\r\n",
                  response_result,
                  response_result_string);
        return 0;
    }

    // Find the sleep timer token in the response
    char sleep_timer_token[TIMER_LENGTH] = "";
    const bool found_token = SequansController.extractValueFromCommandResponse(
        response, TIMER_SLEEP_INDEX, sleep_timer_token, TIMER_LENGTH);

    if (!found_token) {
        Log.warnf("Did not find sleep timer token, got the following: %s\r\n",
                  sleep_timer_token);
        return 0;
    }

    // Shift the pointer one address forward to bypass the quotation mark in
    // the token, since it is returned like this "xxxxxxxx"
    const uint8_t sleep_timer = stringOfBitsToUint8(&(sleep_timer_token[1]));

    // The three first MSB are the multiplier
    const SleepMultiplier sleep_multiplier =
        static_cast<SleepMultiplier>((sleep_timer & PSM_MULTIPLIER_BM) >> 5);

    // The 5 LSB are the value
    const uint8_t sleep_value = sleep_timer & PSM_VALUE_BM;

    return decodeSleepMultiplier(sleep_multiplier) * sleep_value;
}

static void enablePIT(void) {

    uint8_t temp;

    // Disable first and wait for clock to stabilize
    temp = CLKCTRL.XOSC32KCTRLA;
    temp &= ~CLKCTRL_ENABLE_bm;
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, temp);
    while (CLKCTRL.MCLKSTATUS & CLKCTRL_XOSC32KS_bm) {}

    // We want the external crystal to run in standby and in low power mode
    temp = CLKCTRL.XOSC32KCTRLA;
    temp |= CLKCTRL_RUNSTBY_bm | CLKCTRL_LPMODE_bm;
    temp &= ~(CLKCTRL_SEL_bm);
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, temp);

    // Choose to use external crystal on XTAL32K1 and XTAL32K2 pins and enable
    // the clock
    temp = CLKCTRL.XOSC32KCTRLA;
    temp |= CLKCTRL_ENABLE_bm;
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, temp);

    // Wait for registers to synchronize
    while (RTC.PITSTATUS) {}

    RTC.CLKSEL |= RTC_CLKSEL_XOSC32K_gc;
    RTC.PITINTCTRL |= RTC_PI_bm;
    RTC.PITCTRLA |= RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;

    // The first PIT intterupt will not necessarily be at the period specified,
    // so we just wait until it has triggered and track the reminaing time from
    // there
    while (!pit_triggered) {}
    pit_triggered = false;
}

static void disablePIT(void) {

    // Disable external clock and turn off RTC PIT
    uint8_t temp;
    temp = CLKCTRL.XOSC32KCTRLA;
    temp &= ~(CLKCTRL_ENABLE_bm);
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, temp);

    RTC.PITCTRLA &= ~RTC_PITEN_bm;
}

static void powerDownPeripherals(void) {

    cell_led_state = digitalRead(LedCtrl.getLedPin(Led::CELL));
    con_led_state = digitalRead(LedCtrl.getLedPin(Led::CON));

    LedCtrl.off(Led::CELL, true);
    LedCtrl.off(Led::CON, true);
    LedCtrl.off(Led::DATA, true);
    LedCtrl.off(Led::ERROR, true);
    LedCtrl.off(Led::USER, true);
}

static void powerUpPeripherals(void) {
    if (cell_led_state) {
        LedCtrl.on(Led::CELL, true);
    }

    if (con_led_state) {
        LedCtrl.on(Led::CON, true);
    }
}

/**
 * Modem sleeping and CPU deep sleep.
 */
static WakeUpReason regularSleep(void) {

    const unsigned long start_time_ms = millis();

    if (!retrieved_sleep_time) {
        // Retrieve the proper sleep time set by the operator, which may
        // deviate from what we requested
        sleep_time = retrieveOperatorSleepTime();

        if (sleep_time == 0) {
            Log.debugf("Got invalid sleep time: %d\r\n", sleep_time);
            return WakeUpReason::INVALID_SLEEP_TIME;
        } else {
            Log.debugf("Sleep time set to: %d seconds\r\n", sleep_time);
            retrieved_sleep_time = true;
        }
    }

    // The timeout here is arbitrary as we attempt to put the modem in sleep in
    // a loop, so we just choose 30 seconds = 30000 ms
    while (!attemptToEnterPowerSaveModeForModem(30000) &&
           millis() - start_time_ms < sleep_time * 1000) {}

    // If we surpassed the sleep time during setting the LTE to sleep, we
    // don't have any more time to sleep the CPU, so just return.
    if (millis() - start_time_ms >= sleep_time * 1000) {
        return WakeUpReason::MODEM_TIMEOUT;
    }

    enablePIT();

    uint32_t remaining_time_seconds =
        sleep_time - (uint32_t)(((millis() - start_time_ms) / 1000.0f));

    WakeUpReason wakeup_reason = WakeUpReason::OK;

    if (remaining_time_seconds < 0) {
        wakeup_reason = WakeUpReason::MODEM_TIMEOUT;
    }

    // As the PIT timer has a minimum frequency of 1 Hz, we loop over the
    // remaining time and sleep for a second each time before the PIT triggers
    // and interrupt.
    while (remaining_time_seconds > 0) {

        sleep_cpu();

        // Woken up by some external interrupt
        if (!pit_triggered && modem_is_in_power_save) {
            wakeup_reason = WakeUpReason::EXTERNAL_INTERRUPT;
            break;
        }

        // Got woken up by the PIT interrupt
        if (pit_triggered) {
            remaining_time_seconds -= 1;
            pit_triggered = false;
        }

        // Modem caused the CPU to be awoken
        if (!modem_is_in_power_save) {

            if (remaining_time_seconds < PSM_REMAINING_SLEEP_TIME_THRESHOLD) {
                wakeup_reason = WakeUpReason::OK;
                break;
            } else {
                wakeup_reason = WakeUpReason::AWOKEN_BY_MODEM_PREMATURELY;
                break;
            }
        }
    }

    if (modem_is_in_power_save) {
        modem_is_in_power_save = false;
        SequansController.setPowerSaveMode(0, NULL);
    }

    disablePIT();

    return wakeup_reason;
}

/**
 * Modem turned off and CPU deep sleep.
 */
static WakeUpReason deepSleep(void) {

    const unsigned long start_time_ms = millis();

    Lte.end();
    enablePIT();

    uint32_t remaining_time_seconds =
        sleep_time - (uint32_t)(((millis() - start_time_ms) / 1000.0f));

    WakeUpReason wakeup_reason = WakeUpReason::OK;

    while (remaining_time_seconds > 0) {

        sleep_cpu();

        if (pit_triggered) {
            remaining_time_seconds -= 1;
            pit_triggered = false;
        } else {
            wakeup_reason = WakeUpReason::EXTERNAL_INTERRUPT;
            break;
        }
    }

    disablePIT();
    Lte.begin();

    return wakeup_reason;
}

bool LowPowerClass::begin(const SleepMultiplier sleep_multiplier,
                          const uint8_t sleep_value,
                          const SleepMode mode) {

    sleep_mode = mode;

    // Reset in case there is a reconfiguration after sleep has been called
    // previously
    retrieved_sleep_time = false;
    sleep_time = 0;

    // We need sequans controller to be initialized first before configuration
    if (!SequansController.isInitialized()) {
        SequansController.begin();

        // Allow 500ms for boot
        delay(500);
    }

    SequansController.clearReceiveBuffer();

    // First we disable EDRX
    if (!SequansController.retryCommand(AT_COMMAND_DISABLE_EDRX)) {
        return false;
    }

    // Then we set RING behaviour
    if (!SequansController.retryCommand(AT_COMMAND_SET_RING_BEHAVIOUR)) {
        return false;
    }

    // Then we set the power saving mode configuration
    char sleep_parameter_str[9] = "";

    // We encode both the multipier and value in one value after the setup:
    //
    // | Mul | Value |
    // | ... | ..... |
    //
    // Where every dot is one bit
    // Max value is 0b11111 = 31, so we have to clamp to that
    //
    // Need to do casts here as we have strongly typed enum classes to enforce
    // the values we have set for the multipliers

    // First set the mulitplier
    const uint8_t sleep_parameter =
        (static_cast<uint8_t>(sleep_multiplier) << 5) |
        min(sleep_value, PSM_VALUE_MAX);

    uint8ToStringOfBits(sleep_parameter, sleep_parameter_str);

    // Now we can embed the values for the awake and sleep periode in the
    // power saving mode configuration command
    char command[AT_COMMAND_SET_PSM_LENGTH + 1]; // + 1 for null termination
    sprintf(command,
            AT_COMMAND_SET_PSM,
            sleep_parameter_str,
            PSM_DEFAULT_AWAKE_PARAMETER);

    sleep_time = decodeSleepMultiplier(sleep_multiplier) *
                 min(sleep_value, PSM_VALUE_MAX);

    return SequansController.retryCommand(command);
}

WakeUpReason LowPowerClass::sleep(void) {

    powerDownPeripherals();
    SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;

    switch (sleep_mode) {
    case SleepMode::REGULAR:
        return regularSleep();
    case SleepMode::DEEP:
        return deepSleep();
    }

    SLPCTRL.CTRLA &= ~SLPCTRL_SEN_bm;
    powerUpPeripherals();

    return WakeUpReason::OK;
}

#include "low_power.h"

#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "sequans_controller.h"

#include <Arduino.h>
#include <Wire.h>
#include <avr/io.h>
#include <avr/sleep.h>

#define AT_COMMAND_DISABLE_EDRX       "AT+SQNEDRX=0"
#define AT_COMMAND_ENABLE_PSM         "AT+CPSMS=1,,,\"%s\",\"%s\""
#define AT_COMMAND_DISABLE_PSM        "AT+CPSMS=0"
#define AT_COMMAND_SET_RING_BEHAVIOUR "AT+SQNRICFG=1,2,1000"
#define AT_COMMAND_CONNECTION_STATUS  "AT+CEREG?"

// For use of the rest of library, for example low power
#define RESPONSE_CONNECTION_STATUS_SIZE 70

// Command without arguments: 18 bytes
// Both arguments within the quotes are strings of 8 numbers: 8 * 2 = 16 bytes
// Total: 18 + 16 = 34 bytes
#define AT_COMMAND_ENABLE_PSM_LENGTH 34

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

// We set the active/paging timer parameter to ten seconds with this. The reason
// behind this is that we don't care about the active period since after
// sleep has finished, the RTS line will be active and prevent the modem
// from going to sleep and thus the awake time is as long as the RTS
// line is kept active.
//
// This parameter thus only specifies how quickly the modem can go to sleep,
// which we want to be quite low, but long enough time for sufficient paging.
#define PSM_DEFAULT_PAGING_PARAMETER "00000101"

#define PSM_REMAINING_SLEEP_TIME_THRESHOLD 5

// This includes null termination
#define TIMER_LENGTH      11
#define TIMER_SLEEP_INDEX 8

#ifdef __AVR_AVR128DB48__ // MINI

#define RING_PORT   VPORTC
#define RING_PIN_bm PIN6_bm

#define LOWQ_PIN               PIN_PB4
#define VOLTAGE_MEASURE_EN_PIN PIN_PB3
#define VOLTAGE_MEASURE_PIN    PIN_PE0

#define EEPROM_CS_PIN PIN_PE3

#define DEBUGGER_TX_PIN  PIN_PB0
#define DEBUGGER_RX_PIN  PIN_PB1
#define DEBUGGER_LED_PIN PIN_PB2
#define DEBUGGER_SW0_PIN PIN_PD2
#define DEBUGGER_USART   USART3

#define I2C0_SDA_PIN PIN_PC2
#define I2C0_SCL_PIN PIN_PC3
#define I2C1_SDA_PIN PIN_PF2
#define I2C1_SCL_PIN PIN_PF3

#define SPI_CS   PIN_PB5
#define SPI_MOSI PIN_PA4
#define SPI_MISO PIN_PA5
#define SPI_SCK  PIN_PA6

#define SW0_PORT PORTD

#else

#ifdef __AVR_AVR128DB64__ // Non-Mini

#define RING_PORT   VPORTC
#define RING_PIN_bm PIN4_bm

#define LOWQ_PIN               PIN_PB4
#define VOLTAGE_MEASURE_EN_PIN PIN_PB3
#define VOLTAGE_MEASURE_PIN    PIN_PE0

#define EEPROM_CS_PIN PIN_PE3

#define DEBUGGER_TX_PIN  PIN_PB0
#define DEBUGGER_RX_PIN  PIN_PB1
#define DEBUGGER_LED_PIN PIN_PB2
#define DEBUGGER_SW0_PIN PIN_PD2
#define DEBUGGER_USART   USART3

#define I2C0_SDA_PIN PIN_PC2
#define I2C0_SCL_PIN PIN_PC3
#define I2C1_SDA_PIN PIN_PF2
#define I2C1_SCL_PIN PIN_PF3

#define SPI_CS   PIN_PB5
#define SPI_MOSI PIN_PA4
#define SPI_MISO PIN_PA5
#define SPI_SCK  PIN_PA6

#define SW0_PORT PORTD

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

// Singleton. Defined for use of the rest of the library.
LowPowerClass LowPower = LowPowerClass::instance();

static volatile bool ring_line_activity     = false;
static volatile bool modem_is_in_power_save = false;
static volatile bool pit_triggered          = false;

static bool retrieved_period     = false;
static uint32_t period           = 0;
static uint32_t period_requested = 0;

ISR(RTC_PIT_vect) {
    RTC.PITINTFLAGS = RTC_PI_bm;
    pit_triggered   = true;
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

static void uint8ToStringOfBits(const uint8_t value, char* string) {
    // Terminate
    string[8] = 0;

    for (uint8_t i = 0; i < 8; i++) {
        string[i] = (value & (1 << (7 - i))) ? '1' : '0';
    }
}

static uint8_t stringOfBitsToUint8(const char* string) {

    uint8_t value = 0;

    for (uint8_t i = 0; i < 8; i++) {
        // We assume all other values are zero, so we only shift the ones
        if (string[i] == '1') {
            value |= (1 << (7 - i));
        }
    }

    return value;
}

static uint16_t
decodePeriodMultiplier(const PowerSaveModePeriodMultiplier multiplier) {

    switch (multiplier) {
    case PowerSaveModePeriodMultiplier::TEN_HOURS:
        return 36000;
    case PowerSaveModePeriodMultiplier::ONE_HOUR:
        return 3600;
    case PowerSaveModePeriodMultiplier::TEN_MINUTES:
        return 600;
    case PowerSaveModePeriodMultiplier::ONE_MINUTE:
        return 60;
    case PowerSaveModePeriodMultiplier::THIRTY_SECONDS:
        return 30;
    case PowerSaveModePeriodMultiplier::TWO_SECONDS:
        return 2;
    default:
        return 0;
    }
}

/**
 * @brief Will attempt to put the cellular modem in power save mode.
 *
 * @note Will wait for @p waiting_time to see if the modem gets to low power
 * mode.
 *
 * @note The power save mode can be abrupted if a new message arrives from
 * the network (for example a MQTT message). Such messages have to be
 * handled before the cellular modem can be put back in into power save mode.
 *
 * @param waiting_time_ms How long to wait for the modem to get into low
 * power mode before giving up (in milliseconds).
 *
 * @return true if the cellular modem was put in power save mode.
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
            last_time_active   = millis();
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
        response,
        RESPONSE_CONNECTION_STATUS_SIZE);

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

    // Find the period timer token in the response
    char period_timer_token[TIMER_LENGTH] = "";
    const bool found_token = SequansController.extractValueFromCommandResponse(
        response,
        TIMER_SLEEP_INDEX,
        period_timer_token,
        TIMER_LENGTH);

    if (!found_token) {
        Log.warnf("Did not find period timer token, got the following: %s\r\n",
                  period_timer_token);
        return 0;
    }

    // Shift the pointer one address forward to bypass the quotation mark in
    // the token, since it is returned like this "xxxxxxxx"
    const uint8_t period_timer = stringOfBitsToUint8(&(period_timer_token[1]));

    // The three first MSB are the multiplier
    const PowerSaveModePeriodMultiplier psm_period_multiplier =
        static_cast<PowerSaveModePeriodMultiplier>(
            (period_timer & PSM_MULTIPLIER_BM) >> 5);

    // The 5 LSB are the value
    const uint8_t psm_period_value = period_timer & PSM_VALUE_BM;

    return decodePeriodMultiplier(psm_period_multiplier) * psm_period_value;
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
    while (RTC.PITSTATUS) { delay(1); }

    RTC.CLKSEL |= RTC_CLKSEL_XOSC32K_gc;
    RTC.PITINTCTRL |= RTC_PI_bm;
    RTC.PITCTRLA |= RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;

    // The first PIT intterupt will not necessarily be at the period specified,
    // so we just wait until it has triggered and track the reminaing time from
    // there
    while (!pit_triggered) { delay(1); }
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

static void setPinLowPowerMode(const uint8_t pin, const bool pull_up = true) {

    if (pull_up) {
        pinConfigure(pin,
                     PIN_DIR_INPUT | PIN_ISC_DISABLE | PIN_PULLUP_ON |
                         PIN_INPUT_DISABLE);
    } else {
        pinConfigure(pin, PIN_DIR_INPUT | PIN_ISC_DISABLE | PIN_INPUT_DISABLE);
    }
}

static void setPinNormalInputMode(const uint8_t pin) {
    pinConfigure(pin, PIN_DIR_INPUT | PIN_PULLUP_OFF | PIN_INPUT_ENABLE);
}

static void setPinNormalOutputMode(const uint8_t pin) {
    pinConfigure(pin, PIN_DIR_OUTPUT | PIN_PULLUP_OFF | PIN_INPUT_ENABLE);
}

static void powerDownPeripherals(void) {

    // EEPROM - Set in standby mode
    setPinLowPowerMode(EEPROM_CS_PIN);

    // I2C
    //
    // We don't pull up the I2C lines as they have external pull-ups
    Wire.end();
    Wire1.end();
    setPinLowPowerMode(I2C0_SDA_PIN, false);
    setPinLowPowerMode(I2C0_SCL_PIN, false);
    setPinLowPowerMode(I2C1_SDA_PIN, false);
    setPinLowPowerMode(I2C1_SCL_PIN, false);

    // SPI
    setPinLowPowerMode(SPI_CS);
    setPinLowPowerMode(SPI_MOSI);
    setPinLowPowerMode(SPI_MOSI);
    setPinLowPowerMode(SPI_SCK);

    // Debugger, add pull-ups on pins for USART TX & RX, as well as LED pin

    setPinLowPowerMode(DEBUGGER_TX_PIN);
    setPinLowPowerMode(DEBUGGER_RX_PIN);
    setPinLowPowerMode(DEBUGGER_LED_PIN);

    // Only enable pull-up for SW0 to not have current over it, but keep active
    // for waking the device up by the button
    SW0_PORT.PIN2CTRL |= PORT_PULLUPEN_bm;

    // LEDs
    setPinLowPowerMode(LedCtrl.getLedPin(Led::CELL));
    setPinLowPowerMode(LedCtrl.getLedPin(Led::CON));
    setPinLowPowerMode(LedCtrl.getLedPin(Led::DATA));
    setPinLowPowerMode(LedCtrl.getLedPin(Led::ERROR));
    setPinLowPowerMode(LedCtrl.getLedPin(Led::USER));

    // Disable ADC0, used for analogRead
    ADC0.CTRLA &= ~ADC_ENABLE_bm;

    // Voltage measure enable and voltage measure pins have external pull-up
    setPinLowPowerMode(VOLTAGE_MEASURE_EN_PIN, false);
    setPinLowPowerMode(VOLTAGE_MEASURE_PIN, false);

    // Disable millis() timer
    stop_millis();
}

static void powerUpPeripherals() {

    // Enable millis() timer
    restart_millis();

    // Voltage measure
    setPinNormalInputMode(VOLTAGE_MEASURE_PIN);
    setPinNormalOutputMode(VOLTAGE_MEASURE_EN_PIN);

    // ADC for analogRead
    init_ADC0();

    // LEDs
    setPinNormalOutputMode(LedCtrl.getLedPin(Led::CELL));
    setPinNormalOutputMode(LedCtrl.getLedPin(Led::CON));
    setPinNormalOutputMode(LedCtrl.getLedPin(Led::DATA));
    setPinNormalOutputMode(LedCtrl.getLedPin(Led::ERROR));
    setPinNormalOutputMode(LedCtrl.getLedPin(Led::USER));

    // Debugger
    setPinNormalOutputMode(DEBUGGER_TX_PIN);
    setPinNormalInputMode(DEBUGGER_RX_PIN);
    setPinNormalOutputMode(DEBUGGER_LED_PIN);

    SW0_PORT.PIN2CTRL &= ~PORT_PULLUPEN_bm;

    // I2C
    setPinNormalOutputMode(I2C0_SDA_PIN);
    setPinNormalOutputMode(I2C0_SCL_PIN);
    setPinNormalOutputMode(I2C1_SDA_PIN);
    setPinNormalOutputMode(I2C1_SCL_PIN);
    Wire.begin();
    Wire1.begin();

    // SPI
    setPinNormalOutputMode(SPI_CS);
    setPinNormalOutputMode(SPI_MOSI);
    setPinNormalInputMode(SPI_MISO);
    setPinNormalOutputMode(SPI_SCK);

    // EEPROM
    setPinNormalOutputMode(EEPROM_CS_PIN);
}

static void enableLDO(void) {

    pinConfigure(LOWQ_PIN, PIN_DIR_OUTPUT);
    digitalWrite(LOWQ_PIN, HIGH);

    // Wait a little to let LDO mode settle
    delay(100);
}

static void disableLDO(void) {
    pinConfigure(LOWQ_PIN, PIN_DIR_OUTPUT);
    digitalWrite(LOWQ_PIN, LOW);

    // Wait a little to let PWM mode settle
    delay(100);
}

void LowPowerClass::configurePowerDown(void) {

    // We need sequans controller to be initialized first before configuration.
    // This is because we need to disable the PSM mode so that the modem don't
    // do periodic power save, but we can shut it down completely.
    if (!SequansController.isInitialized()) {
        SequansController.begin();
    }

    // Disable EDRX and PSM
    SequansController.writeCommand(AT_COMMAND_DISABLE_EDRX);
    SequansController.writeCommand(AT_COMMAND_DISABLE_PSM);
}

void LowPowerClass::configurePeriodicPowerSave(
    const PowerSaveModePeriodMultiplier power_save_mode_period_multiplier,
    const uint8_t power_save_mode_period_value) {

    // Reset in case there is a reconfiguration after sleep has been called
    // previously
    retrieved_period = false;

    // We need sequans controller to be initialized first before configuration
    if (!SequansController.isInitialized()) {
        SequansController.begin();
    }

    // Disable EDRX as we use PSM
    SequansController.writeCommand(AT_COMMAND_DISABLE_EDRX);

    // Enable RING line so that we can wake up from the power save
    SequansController.writeCommand(AT_COMMAND_SET_RING_BEHAVIOUR);

    // Set the power saving mode configuration
    char period_parameter_str[9] = "";

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
    const uint8_t period_parameter =
        (static_cast<uint8_t>(power_save_mode_period_multiplier) << 5) |
        min(power_save_mode_period_value, PSM_VALUE_MAX);

    uint8ToStringOfBits(period_parameter, period_parameter_str);

    // Now we can embed the values for the awake and sleep periode in the
    // power saving mode configuration command
    char command[AT_COMMAND_ENABLE_PSM_LENGTH + 1]; // + 1 for null termination
    sprintf(command,
            AT_COMMAND_ENABLE_PSM,
            period_parameter_str,
            PSM_DEFAULT_PAGING_PARAMETER);

    period_requested = decodePeriodMultiplier(
                           power_save_mode_period_multiplier) *
                       min(power_save_mode_period_value, PSM_VALUE_MAX);

    SequansController.writeCommand(command);
}

void LowPowerClass::powerSave(void) {

    const uint8_t cell_led_state = digitalRead(LedCtrl.getLedPin(Led::CELL));
    const uint8_t con_led_state  = digitalRead(LedCtrl.getLedPin(Led::CON));

    if (!retrieved_period) {
        // Retrieve the proper sleep time set by the operator, which may
        // deviate from what we requested
        period = retrieveOperatorSleepTime();

        if (period == 0) {
            Log.warnf("Got invalid period from operator: %d\r\n", period);
            return;
        } else {
            if (period_requested != period) {
                Log.warnf("Operator was not able to match the requested power "
                          "save mode period of %d seconds. ",
                          period_requested);
                Log.rawf("Operator sat the period to %d seconds.\r\n", period);
            }

            retrieved_period = true;
        }

        // Retrieving the operator sleep time will call CEREG, which will
        // trigger led ctrl, so we just disable it again.
        LedCtrl.off(Led::CELL, true);
    }
    if (!attemptToEnterPowerSaveModeForModem(30000)) {
        Log.error(
            "Failed to put cellular modem in sleep. Power save functionality "
            "might not be available for your operator.");
    }

    if (modem_is_in_power_save) {
        powerDownPeripherals();
        SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;

        enableLDO();
        sleep_cpu();

        // Will sleep here until we get the RING line activity and wake up
        disableLDO();

        SLPCTRL.CTRLA &= ~SLPCTRL_SEN_bm;
        powerUpPeripherals();

        modem_is_in_power_save = false;
    }

    SequansController.setPowerSaveMode(0, NULL);

    // Pins are active low
    if (!cell_led_state) {
        LedCtrl.on(Led::CELL, true);
    } else {
        LedCtrl.off(Led::CELL, true);
    }

    if (!con_led_state) {
        LedCtrl.on(Led::CON, true);
    } else {
        LedCtrl.off(Led::CON, true);
    }
}

void LowPowerClass::powerDown(const uint32_t power_down_time_seconds) {

    const unsigned long start_time_ms = millis();

    SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;

    Lte.end();
    enablePIT();
    enableLDO();

    uint32_t remaining_time_seconds =
        power_down_time_seconds -
        (uint32_t)(((millis() - start_time_ms) / 1000.0f));

    // Need to power down the peripherals here as we want to grab the time
    // (millis()) from the timer before disabling it
    powerDownPeripherals();

    while (remaining_time_seconds > 0) {

        sleep_cpu();

        if (pit_triggered) {
            remaining_time_seconds -= 1;
            pit_triggered = false;
        } else {
            // External interrupt caused the CPU to wake
            break;
        }
    }

    disableLDO();
    disablePIT();
    SLPCTRL.CTRLA &= ~SLPCTRL_SEN_bm;

    powerUpPeripherals();

    while (!Lte.begin()) {}
}

float LowPowerClass::getSupplyVoltage(void) {

    if (!digitalRead(VOLTAGE_MEASURE_EN_PIN)) {
        pinConfigure(VOLTAGE_MEASURE_EN_PIN, PIN_DIR_OUTPUT);
        digitalWrite(VOLTAGE_MEASURE_EN_PIN, HIGH);
    }

    // The default resolution is 10 bits, so divide by that to get the fraction
    // of VDD, which is 3.3 V
    //
    // The voltage is in a voltage divider, and is divided by 4, so have to
    // multiply it up
    float value = 4.0f * 3.3f * ((float)analogRead(VOLTAGE_MEASURE_PIN)) /
                  1024.0f;

    return value;
}

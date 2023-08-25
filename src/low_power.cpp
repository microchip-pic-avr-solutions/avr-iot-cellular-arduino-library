#include "low_power.h"

#include "flash_string.h"
#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "sequans_controller.h"
#include "timeout_timer.h"

#include <Arduino.h>
#include <Wire.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>

// Max is 0b11111 = 31 for the value of the timers for power saving mode (not
// the multipliers).
#define PSM_VALUE_MAX (31)

// How long time we require the ring line to be stable before declaring that we
// have entered power save mode
#define PSM_RING_LINE_STABLE_THRESHOLD_MS (2500)

// PSM timer specification is given by a 8-bit number with the following format:
//
// | Mul | Value |
// | ... | ..... |
#define PSM_MULTIPLIER_BM (0xE0)
#define PSM_VALUE_BM      (0x1F)

// The following is used to decode the timer value from the operator
#define TIMER_LENGTH      (11)
#define TIMER_SLEEP_INDEX (8)

#define RING_PORT   PORTC
#define RING_PIN_bm PIN6_bm

#define LOWQ_PIN               PIN_PB4
#define VOLTAGE_MEASURE_EN_PIN PIN_PB3
#define VOLTAGE_MEASURE_PIN    PIN_PE0

LowPowerClass LowPower = LowPowerClass::instance();

static const char AT_COMMAND_DISABLE_EDRX[] PROGMEM = "AT+SQNEDRX=0";

/**
 * @brief We set the active/paging timer parameter to ten seconds with this. The
 * reason behind this is that we don't care about the active period since after
 * sleep has finished, the RTS line will be active and prevent the modem
 * from going to sleep and thus the awake time is as long as the RTS
 * line is kept active.
 *
 * This parameter thus only specifies how quickly the modem can go to sleep,
 * which we want to be quite low, but long enough time for sufficient paging.
 */
static const char PSM_DEFAULT_PAGING_PARAMETER[] PROGMEM = "00000101";

static volatile bool ring_line_activity     = false;
static volatile bool modem_is_in_power_save = false;
static volatile bool pit_triggered          = false;

/**
 * @brief Whether we retireved a periode for PSM from the operator.
 */
static bool retrieved_period = false;

/**
 * @brief The period the operator gave for PSM.
 */
static uint32_t period = 0;

/**
 * @brief The periode requested, which might deviate from what the operator
 * gave for PSM.
 */
static uint32_t period_requested = 0;

/**
 * @brief Contains stored values of the PINCTRL register for the respective
 * ports.
 */
static uint8_t pin_ctrl_state[6][8];

/**
 * @brief Contains stored values of the DIR register for the respective ports.
 */
static uint8_t pin_dir_state[6];

/**
 * @brief Contains stored values of the OUT register for the respective ports.
 */
static uint8_t pin_out_state[6];

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

static void uint8ToStringOfBits(const uint8_t value, char string[9]) {
    string[8] = '\0';

    for (uint8_t i = 0; i < 8; i++) {
        string[i] = (value & (1 << (7 - i))) ? '1' : '0';
    }
}

static uint8_t stringOfBitsToUint8(const char* string) {

    uint8_t value = 0;

    for (uint8_t i = 0; i < 8; i++) {
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
 * @note Will wait for @p waiting_time_ms to see if the modem gets to low power
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

    SequansController.setPowerSaveMode(0, NULL);

    // First we make sure the UART buffers are empty on the modem's side so the
    // modem can go to sleep
    do {
        _delay_ms(50);
        SequansController.clearReceiveBuffer();
    } while (SequansController.isRxReady());

    SequansController.setPowerSaveMode(1, ring_line_callback);

    ring_line_activity = false;

    // Now we wait until the ring line to stabilize
    const TimeoutTimer timeout_timer(waiting_time_ms);
    unsigned long last_time_active = millis();

    do {
        // Wait some time before checking the activity on the RING line
        _delay_ms(50);

        if (ring_line_activity || RING_PORT.IN & RING_PIN_bm) {
            last_time_active   = millis();
            ring_line_activity = false;
        }

        if (millis() - last_time_active > PSM_RING_LINE_STABLE_THRESHOLD_MS) {
            modem_is_in_power_save = true;
            return true;
        }
    } while (!timeout_timer.hasTimedOut());

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
    char response[96] = "";

    SequansController.clearReceiveBuffer();
    const ResponseResult response_result = SequansController.writeCommand(
        F("AT+CEREG?"),
        response,
        sizeof(response));

    if (response_result != ResponseResult::OK) {
        char response_result_string[18] = "";
        SequansController.responseResultToString(response_result,
                                                 response_result_string);
        Log.warnf(F("Did not get response result OK when retriving operator "
                    "sleep time: %d, %s\r\n"),
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
        Log.warnf(
            F("Did not find period timer token, got the following: %s\r\n"),
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

/**
 * @brief Enables the periodic interrupt timer which is used in power down mode
 * to count how long along the requested time we've been powered down.
 */
static void enablePIT(void) {

    uint8_t temp;

    // Disable first and wait for oscillator to stabilize
    temp = CLKCTRL.XOSC32KCTRLA;
    temp &= ~CLKCTRL_ENABLE_bm;
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, temp);

    while (CLKCTRL.MCLKSTATUS & CLKCTRL_XOSC32KS_bm) {
        __asm__ __volatile__("nop\n\t");
    }

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

    RTC.CLKSEL |= RTC_CLKSEL_XOSC32K_gc;

    // Wait for registers to synchronize
    while (RTC.PITSTATUS) { __asm__ __volatile__("nop\n\t"); }

    RTC.PITINTCTRL |= RTC_PI_bm;
    RTC.PITCTRLA |= RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;

    // Wait for registers to synchronize
    while (RTC.PITSTATUS) { __asm__ __volatile__("nop\n\t"); }

    // The first PIT intterupt will not necessarily be at the period specified,
    // so we just wait until it has triggered and track the reminaing time from
    // there
    while (!pit_triggered) { __asm__ __volatile__("nop\n\t"); }
    pit_triggered = false;
}

/**
 * @brief Disables the periodic interrupt timer.
 */
static void disablePIT(void) {

    // Disable external oscillator and turn off RTC PIT
    uint8_t temp;
    temp = CLKCTRL.XOSC32KCTRLA;
    temp &= ~(CLKCTRL_ENABLE_bm);
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, temp);

    RTC.PITCTRLA &= ~RTC_PITEN_bm;
}

/**
 * @brief Saves the pin state before powering down the peripherals.
 */
static void savePinState(void) {

    for (uint8_t i = 0; i < 8; i++) {
        pin_ctrl_state[0][i] = *((uint8_t*)&PORTA + 0x10 + i);
    }

    for (uint8_t i = 0; i < 8; i++) {
        pin_ctrl_state[1][i] = *((uint8_t*)&PORTB + 0x10 + i);
    }

    for (uint8_t i = 0; i < 8; i++) {
        pin_ctrl_state[2][i] = *((uint8_t*)&PORTC + 0x10 + i);
    }

    for (uint8_t i = 0; i < 8; i++) {
        pin_ctrl_state[3][i] = *((uint8_t*)&PORTD + 0x10 + i);
    }

    for (uint8_t i = 0; i < 8; i++) {
        pin_ctrl_state[4][i] = *((uint8_t*)&PORTE + 0x10 + i);
    }

    for (uint8_t i = 0; i < 8; i++) {
        pin_ctrl_state[5][i] = *((uint8_t*)&PORTF + 0x10 + i);
    }

    pin_dir_state[0] = PORTA.DIR;
    pin_dir_state[1] = PORTB.DIR;
    pin_dir_state[2] = PORTC.DIR;
    pin_dir_state[3] = PORTD.DIR;
    pin_dir_state[4] = PORTE.DIR;
    pin_dir_state[5] = PORTF.DIR;

    pin_out_state[0] = PORTA.OUT;
    pin_out_state[1] = PORTB.OUT;
    pin_out_state[2] = PORTC.OUT;
    pin_out_state[3] = PORTD.OUT;
    pin_out_state[4] = PORTE.OUT;
    pin_out_state[5] = PORTF.OUT;
}

/**
 * @brief Restores the pin state from the stored buffer.
 */
static void restorePinState(void) {

    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t*)&PORTA + 0x10 + i) = pin_ctrl_state[0][i];
    }

    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t*)&PORTB + 0x10 + i) = pin_ctrl_state[1][i];
    }

    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t*)&PORTC + 0x10 + i) = pin_ctrl_state[2][i];
    }

    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t*)&PORTD + 0x10 + i) = pin_ctrl_state[3][i];
    }

    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t*)&PORTE + 0x10 + i) = pin_ctrl_state[4][i];
    }

    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t*)&PORTF + 0x10 + i) = pin_ctrl_state[5][i];
    }

    PORTA.DIR = pin_dir_state[0];
    PORTB.DIR = pin_dir_state[1];
    PORTC.DIR = pin_dir_state[2];
    PORTD.DIR = pin_dir_state[3];
    PORTE.DIR = pin_dir_state[4];
    PORTF.DIR = pin_dir_state[5];

    PORTA.OUT = pin_out_state[0];
    PORTB.OUT = pin_out_state[1];
    PORTC.OUT = pin_out_state[2];
    PORTD.OUT = pin_out_state[3];
    PORTE.OUT = pin_out_state[4];
    PORTF.OUT = pin_out_state[5];
}

/**
 * @brief Sets the pin state in order to minimize the power consumption. Will
 * not enable LDO.
 *
 * @param keep_modem_active Whether to keep modem lines active (used in PSM).
 */
static void powerDownPeripherals(const bool keep_modem_active) {

    savePinState();

    // For low power, the following configuration should be used.
    // If no comment is specified, the pin is set to input with input buffer
    // disabled and pull-up on

    // On all pins along the feather, interrupts are left on for both edges in
    // case the user wants to wake the device up from external signals. This is
    // also the case for the buttons SW0 and SW1

    // clang-format off

    // Pin - Description            - Comment
    // PA0 - LED0 (CELLULAR)        -
    // PA1 - LED1 (CONNECTION)      -
    // PA2 - LED2 (DATA)            -
    // PA3 - LED3 (ERROR)           -
    // PA4 - SPI0 MOSI (Feather)    -
    // PA5 - SPI0 MISO (Feather)    -
    // PA6 - SPI0 MSCK (Feather)    -
    // PA7 - CLKO (Feather)         -

    // PB0 - USART3 TX              - No pullup, measuring yields lower uA
    // PB1 - USART3 RX              -
    // PB2 - LED4 (USER)            -
    // PB3 - VOLTAGE MEASURE EN     - Output, low, no pullup
    // PB4 - LOWQ EN                - Output, low, no pullup
    // PB5 - SPI0 CS (Feather)      -
    // PB6 - NC                     -
    // PB7 - NC                     -

    // PC0 - USART1 TX (Modem)      -
    // PC1 - USART1 RX (Modem)      -
    // PC2 - I2C0 SDA               - Has external pullup
    // PC3 - I2C0 SCL               - Has external pullup
    // PC4 - CTS0 (Modem)           -
    // PC5 - RESETN (Modem)         - Has external pulldown
    // PC6 - RING0 (modem)          - Source for wake up for PSM
    // PC7 - RTS0 (Modem)           - Has external pullup

    // PD0 - GPIO D9 (Feather)      -
    // PD1 - GPIO A1 (Feather)      -
    // PD2 - SW0 button (Feather)   -
    // PD3 - GPIO A2 (Feather)      -
    // PD4 - GPIO A3 (Feather)      -
    // PD5 - GPIO A4 (Feather)      -
    // PD6 - DAC A0  (Feather)      -
    // PD7 - AREF A5 (Feather)      -

    // PE0 - VMUX Measure           - Not pulled up
    // PE1 - GPIO D6 (Feather)      -
    // PE2 - GPIO D5 (Feather)      -
    // PE3 - SPI0 CS (EEPROM)       - Active low, so nothing extra done here
    // PE4 - NC                     -
    // PE5 - NC                     -
    // PE6 - NC                     -
    // PE7 - NC                     -

    // PF0 - XTAL32K1               - Input buffer not disabled, no pullup. Is used for PIT
    // PF1 - XTAL32K2               - Input buffer not disabled, no pullup. Is used for PIT
    // PF2 - I2C1 SDA (Feather)     - Has external pullup
    // PF3 - I2C1 SCL (Feather)     - Has external pullup
    // PF4 - USART2 TX (Feather)    -
    // PF5 - USART2 RX (Feather)    -
    // PF6 - SW1 button             -
    // PF7 - NC                     -

    // clang-format on

    PORTA.DIR = 0x00;
    PORTB.DIR = PIN3_bm | PIN4_bm;

    if (keep_modem_active) {
        PORTC.DIRCLR = PIN2_bm | PIN3_bm;
    } else {
        PORTC.DIR = 0x00;
    }

    PORTD.DIR = 0x00;
    PORTE.DIR = 0x00;
    PORTF.DIR = 0x00;

    PORTA.OUT = 0x00;
    PORTB.OUT = 0x00;

    if (keep_modem_active) {
        PORTC.OUTCLR = PIN2_bm | PIN3_bm;
    } else {
        PORTC.OUT = 0x00;
    }

    PORTD.OUT = 0x00;
    PORTE.OUT = 0x00;
    PORTF.OUT = 0x00;

    PORTA.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTA.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTA.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTA.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

    PORTB.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTB.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;

    PORTC.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTC.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;

    if (!keep_modem_active) {
        PORTC.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
        PORTC.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
        PORTC.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
        PORTC.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
        PORTC.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
        PORTC.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;
    }

    PORTD.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTD.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

    PORTE.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTE.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTE.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTE.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTE.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTE.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTE.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
    PORTE.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;

    PORTF.PIN0CTRL = 0x00;
    PORTF.PIN1CTRL = 0x00;
    PORTF.PIN2CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTF.PIN3CTRL = PORT_ISC_BOTHEDGES_gc;
    PORTF.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTF.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTF.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTF.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
}

/**
 * @brief Restores the pin configuration which the device had before a power
 * down.
 */
static void powerUpPeripherals() {

    restorePinState();

    // ADC for analogRead
    init_ADC0();
}

/**
 * @brief Enables the LDO regulator, which has a significant lower amp footprint
 * than the standard voltage regulator.
 */
static void enableLDO(void) {

    pinConfigure(LOWQ_PIN, PIN_DIR_OUTPUT | PIN_PULLUP_ON);
    digitalWrite(LOWQ_PIN, HIGH);

    // Wait a little to let LDO mode settle
    _delay_ms(100);
}

/**
 * @brief Disables the LOD after a power down.
 */
static void disableLDO(void) {
    pinConfigure(LOWQ_PIN, PIN_DIR_OUTPUT);
    digitalWrite(LOWQ_PIN, LOW);

    // Wait a little to let PWM mode settle
    _delay_ms(100);
}

/**
 * @brief Configures the Sequans modem for deep sleep by entering manufacturing
 * mode and disabling wake up sources which the low power module does not use.
 * If this is not done, the modem won't go to deep sleep.
 */
static void configureModemForDeepSleep(void) {

    // First we need to enter manufactoring mode to disable wake up sources
    SequansController.writeCommand(F("AT+CFUN=5"));

    // Disable all the wake sources except RTS0 (which is on by default)
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wakeRTS1\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wakeSim0\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wake0\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wake1\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wake2\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wake3\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"wake4\",\"disable\""));

    // Disable the other UARTs on the modem in case the message buffers
    // within these UARTs prevent the modem to sleep
    SequansController.writeCommand(F("AT+SQNHWCFG=\"uart1\",\"disable\""));
    SequansController.writeCommand(F("AT+SQNHWCFG=\"uart2\",\"disable\""));

    // Now we need to issue a reset to get back into regular mode
    SequansController.writeCommand(F("AT^RESET"));

    // Wait for the modem to boot again
    SequansController.waitForURC(F("SYSSTART"));

    // Set device to sleep when RTS0 is pulled high. By default the modem will
    // sleep if RTS0, RTS1 and RTS2 are pulled high, so we want to change that
    SequansController.writeCommand(F("AT+SQNIPSCFG=1,1000"));
}

void LowPowerClass::configurePowerDown(void) {

    // We need sequans controller to be initialized first before configuration.
    // This is because we need to disable the PSM mode so that the modem don't
    // do periodic power save, but we can shut it down completely.
    if (!SequansController.isInitialized()) {
        SequansController.begin();
    }

    configureModemForDeepSleep();

    // Disable EDRX and PSM
    SequansController.writeCommand(FV(AT_COMMAND_DISABLE_EDRX));
    SequansController.writeCommand(F("AT+CPSMS=0"));
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

    configureModemForDeepSleep();

    // Disable EDRX as we use PSM
    SequansController.writeCommand(FV(AT_COMMAND_DISABLE_EDRX));

    // Enable RING line so that we can wake up from the power save
    SequansController.writeCommand(F("AT+SQNRICFG=1,2,1000"));

    // Set the power saving mode configuration. 8-bit number, so 8 + 1 for null
    // termination.
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

    period_requested = decodePeriodMultiplier(
                           power_save_mode_period_multiplier) *
                       min(power_save_mode_period_value, PSM_VALUE_MAX);

    SequansController.writeCommand(F("AT+CPSMS=1,,,\"%s\",\"%S\""),
                                   NULL,
                                   0,
                                   period_parameter_str,
                                   PSM_DEFAULT_PAGING_PARAMETER);
}

void LowPowerClass::powerSave(void) {

    if (!retrieved_period) {
        // Retrieve the proper sleep time set by the operator, which may
        // deviate from what we requested
        period = retrieveOperatorSleepTime();

        if (period == 0) {
            Log.warnf(F("Got invalid period from operator: %d\r\n"), period);
            return;
        } else {
            if (period_requested != period) {
                Log.warnf(
                    F("Operator was not able to match the requested power "
                      "save mode period of %d seconds. "),
                    period_requested);
                Log.rawf(F("Operator set the period to %d seconds.\r\n"),
                         period);
            }

            retrieved_period = true;
        }
    }

    if (!attemptToEnterPowerSaveModeForModem(45000)) {
        Log.error(
            F("Failed to put cellular modem in sleep. Power save functionality "
              "might not be available for your operator."));

        SequansController.setPowerSaveMode(0, NULL);
        return;
    }

    if (modem_is_in_power_save) {
        powerDownPeripherals(true);
        SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;

        enableLDO();

        // Stop millis interrupt
        stop_millis();

        sleep_cpu();

        restart_millis();

        // Will sleep here until we get the RING line activity and wake up
        disableLDO();

        SLPCTRL.CTRLA &= ~SLPCTRL_SEN_bm;
        powerUpPeripherals();

        modem_is_in_power_save = false;
    }

    SequansController.setPowerSaveMode(0, NULL);
}

void LowPowerClass::powerDown(const uint32_t power_down_time_seconds) {

    SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;

    Lte.end();

    powerDownPeripherals(false);

    enablePIT();
    enableLDO();

    // Stop millis interrupt to prevent the timer waking up the core
    stop_millis();

    uint32_t remaining_time_seconds = power_down_time_seconds;

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

    restart_millis();

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
    // of VDD, which is 3.3 V (which logic level at the input pin is at)
    //
    // The voltage is in a voltage divider, and is divided by 4, so have to
    // multiply it up
    float value = 4.0f * 3.3f * ((float)analogRead(VOLTAGE_MEASURE_PIN)) /
                  1023.0f;

    return value;
}

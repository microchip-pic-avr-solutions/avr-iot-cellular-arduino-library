#include "log.h"
#include "low_power.h"
#include "sequans_controller.h"

#include <Arduino.h>

#define AT_COMMAND_DISABLE_EDRX       "AT+SQNEDRX=0"
#define AT_COMMAND_SET_PSM            "AT+CPSMS=1,,,\"%s\",\"%s\""
#define AT_COMMAND_SET_RING_BEHAVIOUR "AT+SQNRICFG=1,2,1000"
#define AT_COMMAND_CONNECTION_STATUS  "AT+CEREG?"

// For use of the rest of library, for example low power
#define RESPONSE_CONNECTION_STATUS_SIZE 70

// Command without arguments: 18 bytes
// Both arguments within the quotes are strings of 8 numbers: 8 * 2 = 16 bytes
// Total: 18 + 16 = 34 bytes
#define AT_COMMAND_SET_PSM_SIZE 34

// Max is 0b11111 = 31 for the value of the timers for power saving mode (not
// the multipliers).
#define PSM_VALUE_MAX 31

// How long we wait between checking the ring line for activity
#define PSM_WAITING_TIME_DELTA_MS 500

// How long time we require the ring line to be stable before declaring that we
// have entered power save mode
#define PSM_RING_LINE_STABLE_THRESHOLD_MS 2000
#define PSM_MULTIPLIER_BM                 0xE0
#define PSM_VALUE_BM                      0x1F

// This includes null termination
#define TIMER_LENGTH       11
#define TIMER_ACTIVE_INDEX 7
#define TIMER_SLEEP_INDEX  8

#define RING_PORT   VPORTC
#define RING_PIN_bm PIN4_bm

// Singleton. Defined for use of the rest of the library.
LowPowerClass LowPower = LowPowerClass::instance();

static void (*sleep_finished_callback)(void) = NULL;

static bool ring_line_activity = false;
static bool is_in_power_save_mode = false;

/**
 * @brief Construct a string of bits from an uint8.
 *
 * @param string (out variable) Has to be preallocated to 9 bits (include
 * termination).
 */
static void uint8ToStringOfBits(const uint8_t value, char *string) {
    // Terminate
    string[8] = 0;

    for (uint8_t i = 0; i < 8; i++) {
        string[i] = (value & (1 << (7 - i))) ? '1' : '0';
    }
}

static uint8_t stringOfBitsToUint8(const char *string) {

    uint8_t value;

    for (uint8_t i = 0; i < 8; i++) {
        // We assume all other values are zero, so we only shift the ones
        if (string[i] == '1') {
            value |= (1 << (7 - i));
        }
    }

    return value;
}

static void ring_line_callback(void) {

    ring_line_activity = true;

    if (is_in_power_save_mode) {
        is_in_power_save_mode = false;

        // We got abrupted, bring the power save mode off such that UART
        // communication is possible again
        SequansController.setPowerSaveMode(0, NULL);

        if (sleep_finished_callback != NULL) {
            sleep_finished_callback();
        }
    }
}

/**
 * @brief Will configure power save mode for the Sequans mode.
 *
 * @note This method has to be called before Lte.begin().
 *
 * @return true If configuration was set successfully.
 */
bool LowPowerClass::begin(const PowerSaveConfiguration power_save_configuration,
                          void (*on_sleep_finished)(void)) {

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
    char sleep_parameter[9] = "";
    char awake_parameter[9] = "";

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
    uint8_t value;

    value =
        (static_cast<uint8_t>(power_save_configuration.sleep_multiplier) << 5) |
        min(power_save_configuration.sleep_value, (uint8_t)PSM_VALUE_MAX);

    uint8ToStringOfBits(value, sleep_parameter);

    value =
        (static_cast<uint8_t>(power_save_configuration.awake_multiplier) << 5) |
        min(power_save_configuration.awake_value, PSM_VALUE_MAX);

    uint8ToStringOfBits(value, awake_parameter);

    // Now we can embed the values for the awake and sleep periode in the
    // power saving mode configuration command
    char command[AT_COMMAND_SET_PSM_SIZE + 1]; // + 1 for null termination
    sprintf(command, AT_COMMAND_SET_PSM, sleep_parameter, awake_parameter);

    sleep_finished_callback = on_sleep_finished;

    return SequansController.retryCommand(command);
}

bool LowPowerClass::attemptToEnterPowerSaveMode(
    const uint32_t waiting_time_ms) {

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

            is_in_power_save_mode = true;
            return true;
        }

    } while (millis() - start_time < waiting_time_ms);

    return false;
}

PowerSaveConfiguration LowPowerClass::getCurrentPowerSaveConfiguration(void) {
    PowerSaveConfiguration power_save_configuration;

    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE];

    ResponseResult response_result = SequansController.readResponse(
        response, RESPONSE_CONNECTION_STATUS_SIZE);

    if (response_result != ResponseResult::OK) {
        char response_result_string[18] = "";
        SequansController.responseResultToString(response_result,
                                                 response_result_string);
        return power_save_configuration;
    }

    // Find the stat token in the response
    char active_timer_token[TIMER_LENGTH];
    bool found_token = SequansController.extractValueFromCommandResponse(
        response, TIMER_ACTIVE_INDEX, active_timer_token, TIMER_LENGTH);

    if (!found_token) {
        Log.warnf(
            "Did not find active/awake timer token, got the following: %s\r\n",
            active_timer_token);
        return power_save_configuration;
    }

    char sleep_timer_token[TIMER_LENGTH];
    found_token = SequansController.extractValueFromCommandResponse(
        response, TIMER_SLEEP_INDEX, sleep_timer_token, TIMER_LENGTH);

    if (!found_token) {
        Log.warnf("Did not find sleep timer token, got the following: %s\r\n",
                  sleep_timer_token);
        return power_save_configuration;
    }

    // Shift the pointer one address forward to bypass the quotation mark in
    // the token
    uint8_t active_timer = stringOfBitsToUint8(&(active_timer_token[1]));
    uint8_t sleep_timer = stringOfBitsToUint8(&(sleep_timer_token[1]));

    // Now we set up the power configuration structure

    power_save_configuration.sleep_multiplier =
        static_cast<SleepUnitMultiplier>((sleep_timer & PSM_MULTIPLIER_BM) >>
                                         5);
    power_save_configuration.awake_multiplier =
        static_cast<AwakeUnitMultiplier>((active_timer & PSM_MULTIPLIER_BM) >>
                                         5);

    power_save_configuration.sleep_value = sleep_timer & PSM_VALUE_BM;
    power_save_configuration.awake_value = active_timer & PSM_VALUE_BM;

    return power_save_configuration;
}

void LowPowerClass::endPowerSaveMode(void) {
    SequansController.setPowerSaveMode(0, NULL);
    is_in_power_save_mode = false;
}

bool LowPowerClass::isInPowerSaveMode(void) { return is_in_power_save_mode; }

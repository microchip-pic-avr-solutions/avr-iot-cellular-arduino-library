#include "log.h"
#include "lte.h"
#include "sequans_controller.h"

#define AT_COMMAND_CONNECT            "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT         "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS  "AT+CEREG?"
#define AT_COMMAND_DISABLE_CEREG_URC  "AT+CEREG=0"
#define AT_COMMAND_ENABLE_CEREG_URC   "AT+CEREG=5"
#define AT_COMMAND_DISABLE_CREG_URC   "AT+CREG=0"
#define AT_COMMAND_DISABLE_EDRX       "AT+SQNEDRX=0"
#define AT_COMMAND_SET_PSM            "AT+CPSMS=1,,,\"%s\",\"%s\""
#define AT_COMMAND_SET_RING_BEHAVIOUR "AT+SQNRICFG=1,2,1000"

// Command without arguments: 18 bytes
// Both arguments within the quotes are strings of 8 numbers: 8 * 2 = 16 bytes
// Total: 18 + 16 = 34 bytes
#define AT_COMMAND_SET_PSM_SIZE 34

#define CEREG_CALLBACK "CEREG"

// This includes null termination
#define STAT_LENGTH                  2
#define STAT_INDEX                   1
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

// This includes null termination
#define TIMER_LENGTH       11
#define TIMER_ACTIVE_INDEX 7
#define TIMER_SLEEP_INDEX  8

#define RESPONSE_CONNECTION_STATUS_SIZE 70

#define CEREG_DATA_LENGTH 2

// When the CEREG appears as an URC, it only includes the stat, but there will
// be a space before the data, hence this value since this index is character
// index.
#define CEREG_STAT_CHARACTER_INDEX 1

// Max is 0b11111 = 31 for the value of the timers for power saving mode (not
// the multipliers).
#define PSM_VALUE_MAX 31

// How long we wait between checking the ring line for activity
#define PSM_WAITING_TIME_DELTA_MS 500

// How long time we require the ring line to be stable before declaring that we
// have entered power save mode
#define PSM_RING_LINE_STABLE_THRESHOLD_MS 2000

#define PSM_MULTIPLIER_BM 0xE0

#define PSM_VALUE_BM 0x1F

#define RING_PORT   VPORTC
#define RING_PIN_bm PIN4_bm

// Singleton. Defined for use of the rest of the library.
LteClass Lte = LteClass::instance();

static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;
static void (*power_save_abrupted_callback)(void) = NULL;

static bool ring_line_activity = false;
static bool is_in_power_save_mode = false;

static void connectionStatus(char *) {
    // +1 for null termination
    char buffer[CEREG_DATA_LENGTH + 1];

    if (SequansController.readNotification(buffer, sizeof(buffer))) {

        const char stat = buffer[CEREG_STAT_CHARACTER_INDEX];

        if (stat == STAT_REGISTERED_ROAMING ||
            stat == STAT_REGISTERED_HOME_NETWORK) {

            if (connected_callback) {
                connected_callback();
            }
        } else {
            if (disconnected_callback) {
                disconnected_callback();
            }
        }
    }
}

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

// template <typename T> static T min(T a, T b) { return a < b ? a : b; }

/**
 * @brief Will configure power save mode for the Sequans mode.
 *
 * @note This method has to be called before begin().
 *
 * @return true If configuration was set successfully.
 */
static bool
configurePowerSaveMode(const PowerSaveConfiguration power_save_configuration) {

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

    return SequansController.retryCommand(command);
}

static void ring_line_callback(void) {

    ring_line_activity = true;

    if (is_in_power_save_mode) {
        is_in_power_save_mode = false;

        // We got abrupted, bring the power save mode off such that UART
        // communication is possible again
        SequansController.setPowerSaveMode(0, NULL);

        if (power_save_abrupted_callback != NULL) {
            power_save_abrupted_callback();
        }
    }
}

void LteClass::begin(const bool enable_power_save,
                     const PowerSaveConfiguration power_save_configuration) {

    SequansController.begin();

    if (enable_power_save) {
        configurePowerSaveMode(power_save_configuration);
    }

    SequansController.clearReceiveBuffer();

    // This might fail the first times after initializing the sequans
    // controller, so we just retry until they succeed
    SequansController.retryCommand(AT_COMMAND_DISABLE_CREG_URC);
    SequansController.retryCommand(AT_COMMAND_ENABLE_CEREG_URC);
    SequansController.retryCommand(AT_COMMAND_CONNECT);

    // This is convenient when the MCU has been issued a reset, but the lte
    // modem is already connected, which will be the case during development for
    // example. In that way, the user gets the callback upon start and doesn't
    // have to check themselves
    if (isConnected() && connected_callback != NULL) {
        connected_callback();
    }
}

void LteClass::end(void) {
    SequansController.retryCommand(AT_COMMAND_DISCONNECT);
    SequansController.end();
}

void LteClass::onConnectionStatusChange(void (*connect_callback)(void),
                                        void (*disconnect_callback)(void)) {
    connected_callback = connect_callback;
    disconnected_callback = disconnect_callback;
    SequansController.registerCallback(CEREG_CALLBACK, connectionStatus);
}

bool LteClass::isConnected(void) {

    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE];

    ResponseResult response_result = SequansController.readResponse(
        response, RESPONSE_CONNECTION_STATUS_SIZE);

    if (response_result != ResponseResult::OK) {
        char response_result_string[18] = "";
        SequansController.responseResultToString(response_result,
                                                 response_result_string);
        Log.warnf("Did not get a valid response when querying CEREG: %s\r\n",
                  response_result_string);
        return false;
    }

    // Find the stat token in the response
    char stat_token[STAT_LENGTH];
    bool found_token = SequansController.extractValueFromCommandResponse(
        response, STAT_INDEX, stat_token, STAT_LENGTH);

    if (!found_token) {
        return false;
    }

    if (stat_token[0] == STAT_REGISTERED_HOME_NETWORK ||
        stat_token[0] == STAT_REGISTERED_ROAMING) {
        return true;
    }

    return false;
}

bool LteClass::attemptToEnterPowerSaveMode(const uint32_t waiting_time_ms) {

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
    uint32_t time_passed_since_ring_activity_ms = 0;
    uint32_t waiting_time_passed_ms = 0;

    do {
        // TODO: should probably change this to use millis()
        delay(PSM_WAITING_TIME_DELTA_MS);

        // Reset timer if there has been activity or the RING line is high
        if (ring_line_activity || RING_PORT.IN & RING_PIN_bm) {
            time_passed_since_ring_activity_ms = 0;
            ring_line_activity = false;
        } else {
            time_passed_since_ring_activity_ms += PSM_WAITING_TIME_DELTA_MS;
        }

        waiting_time_passed_ms += PSM_WAITING_TIME_DELTA_MS;

        if (time_passed_since_ring_activity_ms >
            PSM_RING_LINE_STABLE_THRESHOLD_MS) {

            is_in_power_save_mode = true;
            return true;
        }

    } while (waiting_time_passed_ms < waiting_time_ms);

    return false;
}

PowerSaveConfiguration LteClass::getCurrentPowerSaveConfiguration(void) {
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
        // TODO: Should this be included?
        Log.warnf(
            "Did not find active/awake timer token, got the following: %s\r\n",
            active_timer_token);
        return power_save_configuration;
    }

    char sleep_timer_token[TIMER_LENGTH];
    found_token = SequansController.extractValueFromCommandResponse(
        response, TIMER_SLEEP_INDEX, sleep_timer_token, TIMER_LENGTH);

    if (!found_token) {
        // TODO: Should this be included?
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

void LteClass::endPowerSaveMode(void) {
    SequansController.setPowerSaveMode(0, NULL);
    is_in_power_save_mode = false;
}

void LteClass::onPowerSaveAbrupted(void (*callback)(void)) {
    power_save_abrupted_callback = callback;
}

bool LteClass::isInPowerSaveMode(void) { return is_in_power_save_mode; }

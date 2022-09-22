#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "mqtt_client.h"
#include "sequans_controller.h"

#include <Arduino.h>

#define AT_CONNECT                    "AT+CFUN=1"
#define AT_DISCONNECT                 "AT+CFUN=0"
#define AT_CONNECTION_STATUS          "AT+CEREG?"
#define AT_ENABLE_CEREG_URC           "AT+CEREG=5"
#define AT_CHECK_SIM                  "AT+CPIN?"
#define AT_QUERY_OPERATOR_SET_FORMAT  "AT+COPS=3,0"
#define AT_QUERY_OPERATOR             "AT+COPS?"
#define AT_ENABLE_TIME_ZONE_UPDATE    "AT+CTZU=1"
#define AT_ENABLE_TIME_ZONE_REPORTING "AT+CTZR=1"
#define AT_GET_CLOCK                  "AT+CCLK?"
#define AT_SYNC_NTP \
    "AT+SQNNTP=2,\"time.google.com,time.windows.com,pool.ntp.org\",1"

#define CEREG_CALLBACK    "CEREG"
#define TIMEZONE_CALLBACK "CTZV"
#define NTP_CALLBACK      "SQNNTP"

#define TIMEZONE_WAIT_MS 10000

// This includes null termination
#define STAT_LENGTH                  2
#define STAT_INDEX                   1
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

#define NTP_STATUS_INDEX 1
#define NTP_OK           '0'

#define RESPONSE_CONNECTION_STATUS_SIZE 70

#define CEREG_DATA_LENGTH 2

// When the CEREG appears as an URC, it only includes the stat, but there will
// be a space before the data, hence this value since this index is character
// index.
#define CEREG_STAT_CHARACTER_INDEX 1

const char* OPERATOR_NOT_AVAILABLE = "NOT_AVAILABLE";

/**
 * @brief Singleton. Defined for use of the rest of the library.
 */
LteClass Lte = LteClass::instance();

/**
 * @brief Function pointer to the user's disconnect callback (if registered).
 */
static void (*disconnected_callback)(void) = NULL;

/**
 * @brief Keeps track of the connection status.
 */
static volatile bool is_connected = false;

/**
 * @brief Keeps track of whether we've received the timezone URC.
 */
static volatile bool got_timezone = false;

static void connectionStatus(char* buffer) {

    const char stat = buffer[CEREG_STAT_CHARACTER_INDEX];

    if (stat == STAT_REGISTERED_ROAMING ||
        stat == STAT_REGISTERED_HOME_NETWORK) {

        is_connected = true;

        LedCtrl.on(Led::CELL, true);

    } else {

        if (is_connected) {
            is_connected = false;
            LedCtrl.off(Led::CELL, true);

            // The modem does not give any notification of a MQTT disconnect.
            // This must be called directly following a connection loss
            MqttClient.end();

            if (disconnected_callback != NULL) {
                disconnected_callback();
            }
        }
    }
}

static void timezoneCallback(__attribute__((unused)) char* buffer) {
    got_timezone = true;
}

bool LteClass::begin(const bool print_messages) {

    // If low power is utilized, sequans controller will already been
    // initialized, so don't reset it by calling begin again
    if (!SequansController.isInitialized()) {
        SequansController.begin();
    }

    SequansController.registerCallback(TIMEZONE_CALLBACK, timezoneCallback);

    SequansController.writeCommand(AT_ENABLE_TIME_ZONE_UPDATE);
    SequansController.writeCommand(AT_ENABLE_TIME_ZONE_REPORTING);
    SequansController.writeCommand(AT_ENABLE_CEREG_URC);
    SequansController.writeCommand(AT_CONNECT);

    char response_buffer[48] = "";
    char value_buffer[32]    = "";

    // Wait for CEREG URC before checking SIM
    SequansController.waitForURC(CEREG_CALLBACK);

    // We check that the SIM card is inserted and ready. Note that we can only
    // do this and get a meaningful response in CFUN=1 or CFUN=4.
    if (SequansController.writeCommand(AT_CHECK_SIM,
                                       response_buffer,
                                       sizeof(response_buffer)) !=
        ResponseResult::OK) {
        Log.error("Checking SIM card failed, is it inserted?");
        Lte.end();

        return false;
    }

    if (!SequansController.extractValueFromCommandResponse(
            response_buffer,
            0,
            value_buffer,
            sizeof(value_buffer))) {
        Log.error("Failed to retrieve SIM status");
        Lte.end();

        return false;
    }

    if (strncmp(value_buffer, "READY", 5) != 0) {
        Log.errorf("SIM card is not ready, status: %s", value_buffer);
        Lte.end();

        return false;
    }

    SequansController.registerCallback(CEREG_CALLBACK, connectionStatus, false);

    if (print_messages) {
        Log.infof("Connecting to operator");
    }

    while (!isConnected()) {
        LedCtrl.toggle(Led::CELL, true);
        delay(500);

        if (print_messages) {
            Log.rawf(".");
        }
    }

    if (print_messages) {
        Log.rawf(" OK!\r\n");
    }

    if (SequansController.writeCommand(AT_GET_CLOCK,
                                       response_buffer,
                                       sizeof(response_buffer)) !=
        ResponseResult::OK) {

        Log.error("Command for retrieving modem time failed");
        Lte.end();

        return false;
    }

    if (!SequansController.extractValueFromCommandResponse(
            response_buffer,
            0,
            value_buffer,
            sizeof(value_buffer))) {
        Log.error("Failed to retrieve time from modem");
        Lte.end();

        return false;
    }

    char year[3]  = "";
    char month[3] = "";
    char day[3]   = "";
    memcpy(year, &value_buffer[0] + 1, 2);
    memcpy(month, &value_buffer[0] + 4, 2);
    memcpy(day, &value_buffer[0] + 7, 2);

    // We check the date and whether it is unix epoch start or not
    if (atoi(year) == 70 && atoi(month) == 1 && atoi(day) == 1) {

        // Not valid time, have to do sync. First we wait some to see if we get
        // the timezone URC
        const unsigned long start = millis();
        while (millis() - start < TIMEZONE_WAIT_MS && !got_timezone) {}

        if (!got_timezone) {
            // Do manual sync with NTP server

            if (print_messages) {
                Log.info("Did not get time from operator, doing NTP sync. "
                         "This can take some time...");
            }

            // Will break from this when we get the NTP sync
            while (true) {

                // We might be disconnected from the network whilst doing the
                // NTP sync, so return if that is the case
                if (!isConnected()) {
                    Lte.end();
                    Log.warn(
                        "Got disconnected from network whilst doing NTP sync");
                    return false;
                }

                if (SequansController.writeCommand(AT_SYNC_NTP) !=
                    ResponseResult::OK) {
                    continue;
                }

                char buffer[64] = "";

                if (!SequansController.waitForURC(NTP_CALLBACK,
                                                  buffer,
                                                  sizeof(buffer))) {
                    // Time wait for the NTP URC timed out, retry
                    continue;
                }

                if (buffer[NTP_STATUS_INDEX] == NTP_OK) {
                    Log.info("Got NTP sync!");
                    break;
                }
            }
        }
    }

    SequansController.unregisterCallback(TIMEZONE_CALLBACK);

    return true;
}

void LteClass::end(void) {

    if (SequansController.isInitialized()) {

        SequansController.unregisterCallback(TIMEZONE_CALLBACK);
        SequansController.writeCommand(AT_DISCONNECT);

        // Wait for the CEREG URC after disconnect so that the modem doesn't
        // have any pending URCs and won't prevent going to sleep
        uint32_t start = millis();
        while (isConnected() && millis() - start < 2000) {}
        SequansController.unregisterCallback(CEREG_CALLBACK);

        SequansController.end();
    }

    got_timezone = false;
    is_connected = false;
}

String LteClass::getOperator(void) {

    char response[64] = "";
    char id[48]       = "";

    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_QUERY_OPERATOR_SET_FORMAT);

    SequansController.clearReceiveBuffer();

    if (SequansController.writeCommand(AT_QUERY_OPERATOR,
                                       response,
                                       sizeof(response)) !=
        ResponseResult::OK) {

        Log.error("Failed to query the operator name");
        return OPERATOR_NOT_AVAILABLE;
    }

    if (!SequansController.extractValueFromCommandResponse(response,
                                                           2,
                                                           id,
                                                           sizeof(id))) {

        Log.error("Failed to retrieve the operator name");
        return OPERATOR_NOT_AVAILABLE;
    }

    // Remove the quotes
    char* buffer            = id + 1;
    id[strnlen(buffer, 47)] = '\0';

    return String(buffer);
}

void LteClass::onDisconnect(void (*disconnect_callback)(void)) {
    disconnected_callback = disconnect_callback;
}

bool LteClass::isConnected(void) { return is_connected; }

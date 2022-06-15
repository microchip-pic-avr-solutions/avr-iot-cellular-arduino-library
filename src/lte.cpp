#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "mqtt_client.h"
#include "sequans_controller.h"

#include <Arduino.h>

#define AT_COMMAND_CONNECT                                    "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT                                 "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS                          "AT+CEREG?"
#define AT_COMMAND_ENABLE_CEREG_URC                           "AT+CEREG=5"
#define AT_COMMAND_CHECK_SIM                                  "AT+CPIN?"
#define AT_COMMAND_QUERY_OPERATOR_SET_FORMAT                  "AT+COPS=3,0"
#define AT_COMMAND_QUERY_OPERATOR                             "AT+COPS?"
#define AT_COMMAND_ENABLE_AUTOMATIC_TIME_UPDATE               "AT+CTZU=1"
#define AT_COMMAND_ENABLE_AUTOMATIC_TIME_UPDATE_NOTIFICATIONS "AT+CTZR=1"
#define AT_COMMAND_GET_CLOCK                                  "AT+CCLK?"
#define AT_COMMAND_SYNC_NTP_ON_DEMAND                         "AT+SQNNTP=0"
#define AT_COMMAND_SYNC_NTP                                   "AT+SQNNTP=2"

#define CEREG_CALLBACK    "CEREG"
#define TIMEZONE_CALLBACK "CTZV"
#define NTP_CALLBACK      "SQNNTP"

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

const char *OPERATOR_NOT_AVAILABLE = "NOT_AVAILABLE";

// Singleton. Defined for use of the rest of the library.
LteClass Lte = LteClass::instance();

static void (*disconnected_callback)(void) = NULL;
static volatile bool is_connected = false;
static volatile bool got_timezone = false;
static volatile bool got_ntp_callback = false;
static volatile bool got_ntp_sync = false;

static void connectionStatus(char *buffer) {

    const char stat = buffer[CEREG_STAT_CHARACTER_INDEX];

    if (stat == STAT_REGISTERED_ROAMING ||
        stat == STAT_REGISTERED_HOME_NETWORK) {

        is_connected = true;

        // Guard for the first time the LTE modem is connected and we check the
        // time synchronization. We don't want to fire the callback to the user
        // before we have made sure that the time is synchronized
        if (!got_ntp_sync) {
            return;
        }

        LedCtrl.on(Led::CELL, true);
    } else {

        if (is_connected) {
            is_connected = false;
            LedCtrl.off(Led::CELL, true);

            // The modem does not give any notification of a MQTT disconnect.
            // This must be called directly following a connection loss
            MqttClient.disconnect(true);

            if (disconnected_callback != NULL) {
                disconnected_callback();
            }
        }
    }
}

static void timezoneCallback(char *buffer) { got_timezone = true; }

static void ntpCallback(char *buffer) {
    got_ntp_callback = true;

    // Check that the status is 0, which signifies that the NTP sync was OK.
    if (buffer[NTP_STATUS_INDEX] == NTP_OK) {
        got_ntp_sync = true;
    }
}

bool LteClass::begin(const bool print_messages) {

    // If low power is utilized, sequans controller will already been
    // initialized, so don't reset it by calling begin again
    if (!SequansController.isInitialized()) {
        SequansController.begin();

        // Allow 500ms for boot
        delay(500);
    }

    SequansController.clearReceiveBuffer();

    SequansController.retryCommand(AT_COMMAND_ENABLE_AUTOMATIC_TIME_UPDATE);
    SequansController.retryCommand(
        AT_COMMAND_ENABLE_AUTOMATIC_TIME_UPDATE_NOTIFICATIONS);

    SequansController.registerCallback(TIMEZONE_CALLBACK, timezoneCallback);

    SequansController.retryCommand(AT_COMMAND_ENABLE_CEREG_URC);
    SequansController.retryCommand(AT_COMMAND_CONNECT);

    // CPIN might fail if issued to quickly after CFUN, so delay some
    delay(500);

    // Clear receive buffer before querying the SIM card
    SequansController.clearReceiveBuffer();

    // We check that the SIM card is inserted and ready. Note that we can only
    // do this and get a meaningful response in CFUN=1 or CFUN=4.
    SequansController.retryCommand(AT_COMMAND_CHECK_SIM);

    char response[32] = "";

    ResponseResult result =
        SequansController.readResponse(response, sizeof(response));

    if (result != ResponseResult::OK) {
        Log.error("Checking SIM status failed, is the SIM card inserted?");
        SequansController.retryCommand(AT_COMMAND_DISCONNECT);
        return false;
    }

    char sim_status[16] = "";

    if (!SequansController.extractValueFromCommandResponse(
            response, 0, sim_status, sizeof(sim_status))) {
        Log.error("Failed to extract value from command response during SIM "
                  "status check");
        SequansController.retryCommand(AT_COMMAND_DISCONNECT);
        return false;
    }

    // strncmp returns 0 if the strings are equal
    if (strncmp(sim_status, "READY", 5)) {
        Log.errorf("SIM card is not ready, error: %s\r\n", sim_status);
        SequansController.retryCommand(AT_COMMAND_DISCONNECT);
        return false;
    }

    // Wait for connection before we can check the time synchronization
    SequansController.registerCallback(CEREG_CALLBACK, connectionStatus, false);

    if (print_messages) {
        Log.infof("Connecting to operator");
    }

    while (!isConnected()) {
        delay(500);

        if (print_messages) {
            Log.rawf(".");
        }
    }

    if (print_messages) {
        Log.rawf(" OK!\r\n");
    }

    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_COMMAND_GET_CLOCK);

    char clock_response[48] = "";
    result =
        SequansController.readResponse(clock_response, sizeof(clock_response));

    if (result != ResponseResult::OK) {
        Log.errorf("Retrieving LTE modem time failed");
        SequansController.retryCommand(AT_COMMAND_DISCONNECT);
        return false;
    }

    char time[32] = "";

    if (!SequansController.extractValueFromCommandResponse(
            clock_response, 0, time, sizeof(time))) {
        Log.error("Failed to extract current time from LTE modem");
        SequansController.retryCommand(AT_COMMAND_DISCONNECT);
        return false;
    }

    char year[3] = "";
    char month[3] = "";
    char day[3] = "";
    memcpy(year, &time[0] + 1, 2);
    memcpy(month, &time[0] + 4, 2);
    memcpy(day, &time[0] + 7, 2);

    // We check the date and whether it is unix epoch start or not
    if (atoi(year) == 70 && atoi(month) == 1 && atoi(day) == 1) {

        // Not valid time, have to do sync
        unsigned long start = millis();
        while (start - millis() > 4000 && !got_timezone) {}

        if (!got_timezone) {
            // Do manual sync with NTP server

            if (print_messages) {
                Log.info("Did not get time synchronization from operator, "
                         "doing NTP synchronization. This can take some time.");
            }

            SequansController.clearReceiveBuffer();
            SequansController.registerCallback(NTP_CALLBACK, ntpCallback);

            while (!got_ntp_sync) {
                SequansController.clearReceiveBuffer();
                SequansController.retryCommand(AT_COMMAND_SYNC_NTP);

                if (print_messages) {
                    Log.infof("Waiting for NTP sync");
                }

                while (!got_ntp_callback) {

                    if (print_messages) {
                        Log.rawf(".");
                    }

                    delay(500);
                }

                if (got_ntp_sync) {
                    break;
                } else {
                    if (print_messages) {
                        Log.rawf("\r\n");
                        Log.info("NTP synchronization timed out, retrying...");
                    }
                    got_ntp_callback = false;
                }
            }

            if (print_messages) {
                Log.rawf(" OK!\r\n");
            }

            SequansController.unregisterCallback(NTP_CALLBACK);
        }
    }

    return true;
}

void LteClass::end(void) {
    is_connected = false;
    got_timezone = false;
    got_ntp_sync = false;
    got_ntp_callback = false;

    SequansController.unregisterCallback(CEREG_CALLBACK);
    SequansController.unregisterCallback(TIMEZONE_CALLBACK);
    SequansController.retryCommand(AT_COMMAND_DISCONNECT);
    SequansController.end();
}

String LteClass::getOperator(void) {

    char response[64] = "";
    char id[48] = "";

    SequansController.clearReceiveBuffer();
    SequansController.retryCommand(AT_COMMAND_QUERY_OPERATOR_SET_FORMAT);
    SequansController.retryCommand(AT_COMMAND_QUERY_OPERATOR);

    ResponseResult response_result =
        SequansController.readResponse(response, sizeof(response));

    if (response_result != ResponseResult::OK) {
        Log.errorf("Failed to query the operator, error code: %d\r\n",
                   response);
        return OPERATOR_NOT_AVAILABLE;
    }

    if (!SequansController.extractValueFromCommandResponse(
            response, 2, id, sizeof(id))) {
        Log.error("Failed to extract value during operator query");
        return OPERATOR_NOT_AVAILABLE;
    }

    // Remove the quotes
    char *buffer = id + 1;
    id[strnlen(buffer, 47)] = '\0';

    return String(buffer);
}

void LteClass::onDisconnect(void (*disconnect_callback)(void)) {
    disconnected_callback = disconnect_callback;
}

bool LteClass::isConnected(void) { return is_connected; }

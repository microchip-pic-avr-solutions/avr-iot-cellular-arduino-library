#include "lte.h"

#include "flash_string.h"
#include "led_ctrl.h"
#include "log.h"
#include "mqtt_client.h"
#include "sequans_controller.h"
#include "timeout_timer.h"

#include <Arduino.h>
#include <util/delay.h>

#define TIMEZONE_WAIT_MS 10000

#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

#define NTP_STATUS_INDEX 1
#define NTP_OK           '0'

// When the CEREG appears as an URC, it only includes the stat, but there will
// be a space before the data, hence this value since this index is character
// index.
#define CEREG_STAT_CHARACTER_INDEX 1

const char AT_DISCONNECT[] PROGMEM     = "AT+CFUN=0";
const char CEREG_CALLBACK[] PROGMEM    = "CEREG";
const char TIMEZONE_CALLBACK[] PROGMEM = "CTZV";

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

bool LteClass::begin(const uint32_t timeout_ms, const bool print_messages) {

    const TimeoutTimer timeout_timer(timeout_ms);

    // If low power is utilized, the modem will already be initialized, so don't
    // reset it by calling begin again
    if (!SequansController.isInitialized()) {
        if (!SequansController.begin()) {
            return false;
        }
    }

    // Disconnect before configuration if already connected
    SequansController.writeCommand(FV(AT_DISCONNECT));

    // Enable time zone callback
    SequansController.registerCallback(FV(TIMEZONE_CALLBACK), timezoneCallback);

    // Enable time zone update
    SequansController.writeCommand(F("AT+CTZU=1"));

    // Enable time zone reporting
    SequansController.writeCommand(F("AT+CTZR=1"));

    // Enable CEREG URC
    SequansController.writeCommand(F("AT+CEREG=5"));

    // Start connecting to the operator
    SequansController.writeCommand(F("AT+CFUN=1"));

    char response_buffer[64] = "";
    char value_buffer[32]    = "";

    // Wait for initial CEREG URC before checking SIM
    SequansController.waitForURC(FV(CEREG_CALLBACK));

    SequansController.registerCallback(FV(CEREG_CALLBACK),
                                       connectionStatus,
                                       false);

    // We check that the SIM card is inserted and ready. Note that we can only
    // do this and get a meaningful response in CFUN=1 or CFUN=4.
    if (SequansController.writeCommand(F("AT+CPIN?"),
                                       response_buffer,
                                       sizeof(response_buffer)) !=
        ResponseResult::OK) {
        Log.error(F("Checking SIM card failed, is it inserted?"));
        Lte.end();

        return false;
    }

    if (!SequansController.extractValueFromCommandResponse(
            response_buffer,
            0,
            value_buffer,
            sizeof(value_buffer))) {
        Log.error(F("Failed to retrieve SIM status."));
        Lte.end();

        return false;
    }

    if (strncmp_P(value_buffer, PSTR("READY"), 5) != 0) {
        Log.errorf(F("SIM card is not ready, status: %s."), value_buffer);
        Lte.end();

        return false;
    }

    if (print_messages) {
        Log.infof(F("Connecting to operator"));
    }

    while (!isConnected() && !timeout_timer.hasTimedOut()) {
        LedCtrl.toggle(Led::CELL, true);
        _delay_ms(500);

        if (print_messages) {
            Log.rawf(F("."));
        }
    }

    if (!isConnected()) {
        const char* error_message = PSTR(
            "Was not able to connect to the network within the timeout "
            "of %d ms. Consider increasing the timeout or checking your "
            "cellular coverage.\r\n");

        if (print_messages) {
            Log.rawf(F(" ERROR: %S\r\n"), error_message);
        } else {
            Log.errorf(F("%S\r\n"), error_message);
        }

        Lte.end();

        return false;
    }

    if (print_messages) {
        Log.rawf(" OK!\r\n");
    }

    // Get the time from the modem
    if (SequansController.writeCommand(F("AT+CCLK?"),
                                       response_buffer,
                                       sizeof(response_buffer)) !=
        ResponseResult::OK) {

        Log.error(F("Command for retrieving modem time failed"));
        Lte.end();

        return false;
    }

    if (!SequansController.extractValueFromCommandResponse(
            response_buffer,
            0,
            value_buffer,
            sizeof(value_buffer))) {

        Log.error(F("Failed to retrieve time from modem"));
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
        const TimeoutTimer timezone_timer(TIMEZONE_WAIT_MS);

        while (!timezone_timer.hasTimedOut() && !got_timezone) {}

        if (!got_timezone) {

            // Do manual sync with NTP server
            if (print_messages) {
                Log.info(F("Did not get time from operator, doing NTP sync. "
                           "This can take some time..."));
            }

            // Will break from this when we get the NTP sync

            const TimeoutTimer ntp_sync_timer(timeout_ms);
            bool got_ntp_sync = false;

            while (!ntp_sync_timer.hasTimedOut() && !got_ntp_sync) {

                // We might be disconnected from the network whilst doing the
                // NTP sync, so return if that is the case
                if (!isConnected()) {
                    Log.warn(F(
                        "Got disconnected from network whilst doing NTP sync"));
                    Lte.end();
                    return false;
                }

                // Perform the actual NTP sync
                if (SequansController.writeCommand(
                        F("AT+SQNNTP=2,\"time.google.com,time.windows.com,pool."
                          "ntp.org\",1")) != ResponseResult::OK) {
                    continue;
                }

                char buffer[64] = "";

                if (!SequansController.waitForURC(F("SQNNTP"),
                                                  buffer,
                                                  sizeof(buffer))) {
                    // Wait for the NTP URC timed out, retry
                    continue;
                }

                if (buffer[NTP_STATUS_INDEX] == NTP_OK) {
                    Log.info(F("Got NTP sync!"));
                    got_ntp_sync = true;
                    break;
                }
            }

            if (!got_ntp_sync) {
                Log.warnf(F("Did not get NTP sync within timeout of %lu ms. "
                            "Consider increasing timeout for Lte.begin()\r\n"),
                          timeout_ms);
                Lte.end();
                return false;
            }
        }
    }

    SequansController.unregisterCallback(FV(TIMEZONE_CALLBACK));

    return true;
}

void LteClass::end(void) {

    if (SequansController.isInitialized()) {

        // Terminate active connections (if any) so that we don't suddenly get a
        // hanging URC preventing the modem to shut down
        MqttClient.end();

        SequansController.unregisterCallback(FV(TIMEZONE_CALLBACK));
        SequansController.writeCommand(FV(AT_DISCONNECT));

        // Wait for the CEREG URC after disconnect so that the modem doesn't
        // have any pending URCs and won't prevent going to sleep
        const TimeoutTimer timeout_timer(2000);
        while (isConnected() && !timeout_timer.hasTimedOut()) {}

        SequansController.unregisterCallback(FV(CEREG_CALLBACK));

        SequansController.clearReceiveBuffer();
        SequansController.end();
    }

    got_timezone = false;
    is_connected = false;
}

String LteClass::getOperator(void) {

    char response[64] = "";
    char id[48]       = "";

    // Set human readable format for operator query
    SequansController.writeCommand(F("AT+COPS=3,0"));

    // Query operator name
    if ((SequansController.writeCommand(F("AT+COPS?"),
                                        response,
                                        sizeof(response)) !=
         ResponseResult::OK) ||
        (!SequansController
              .extractValueFromCommandResponse(response, 2, id, sizeof(id)))) {

        Log.error(F("Failed to retrieve the operator name."));
        return String(F("NOT_AVAILABLE"));
    }

    // Remove the quotes
    char* buffer                        = id + 1;
    id[strnlen(buffer, sizeof(id) - 1)] = '\0';

    return String(buffer);
}

void LteClass::onDisconnect(void (*disconnect_callback)(void)) {
    disconnected_callback = disconnect_callback;
}

bool LteClass::isConnected(void) { return is_connected; }

#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "mqtt_client.h"
#include "sequans_controller.h"

#include <Arduino.h>

#define AT_COMMAND_CONNECT             "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT          "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS   "AT+CEREG?"
#define AT_COMMAND_ENABLE_CEREG_URC    "AT+CEREG=5"
#define AT_COMMAND_CHECK_SIM           "AT+CPIN?"
#define AT_COMMAND_QUERY_OPERATOR      "AT+COPS?"
#define AT_COMMAND_QUERY_OPERATOR_LIST "AT+COPN"

#define CEREG_CALLBACK "CEREG"

// This includes null termination
#define STAT_LENGTH                  2
#define STAT_INDEX                   1
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

#define RESPONSE_CONNECTION_STATUS_SIZE 70

#define CEREG_DATA_LENGTH 2

// When the CEREG appears as an URC, it only includes the stat, but there will
// be a space before the data, hence this value since this index is character
// index.
#define CEREG_STAT_CHARACTER_INDEX 1

const char *OPERATOR_NOT_AVAILABLE = "NOT_AVAILABLE";

// Singleton. Defined for use of the rest of the library.
LteClass Lte = LteClass::instance();

static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;
volatile static bool is_connected = false;

static void connectionStatus(char *buffer) {

    const char stat = buffer[CEREG_STAT_CHARACTER_INDEX];

    if (stat == STAT_REGISTERED_ROAMING ||
        stat == STAT_REGISTERED_HOME_NETWORK) {

        is_connected = true;
        LedCtrl.on(Led::CELL, true);

        if (connected_callback != NULL) {
            connected_callback();
        }
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

bool LteClass::begin(void) {

    // If low power is utilized, sequans controller will already been
    // initialized, so don't reset it by calling begin again
    if (!SequansController.isInitialized()) {
        SequansController.begin();

        // Allow 500ms for boot
        delay(500);
    }

    SequansController.clearReceiveBuffer();

    SequansController.retryCommand(AT_COMMAND_ENABLE_CEREG_URC);
    SequansController.retryCommand(AT_COMMAND_CONNECT);

    // CPIN might fail if issued to quickly after CFUN
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

    // Enable the default callback
    SequansController.registerCallback(CEREG_CALLBACK, connectionStatus, false);

    // This is convenient when the MCU has been issued a reset, but the lte
    // modem is already connected, which will be the case during development for
    // example. In that way, the user gets the callback upon start and doesn't
    // have to check themselves
    if (isConnected() && connected_callback != NULL) {
        connected_callback();
    }

    return true;
}

void LteClass::end(void) {
    SequansController.unregisterCallback(CEREG_CALLBACK);
    SequansController.retryCommand(AT_COMMAND_DISCONNECT);
    SequansController.end();
}

String LteClass::getOperator(void) {

    char response[48] = "";
    char id[16] = "";

    SequansController.clearReceiveBuffer();
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

    // Now we have the ID of the operator. We then need to scan through the list
    // in order to find the corresponding name.

    // Here we do things more manually as we receive a long list of operator
    // names, we can't query, so we have to do a manual search.
    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_COMMAND_QUERY_OPERATOR_LIST);

    while (!SequansController.isRxReady()) {}

    // The format is:
    // +COPN: "<id1>","<name1>"
    // +COPN: "<id2>","<name2>"
    // +COPN: "<id3>","<name3>"
    // ...
    // OK

    // Read each line of COPN and check the id against our operator's id
    uint8_t index = 0;
    char buffer[48] = "";

    while (SequansController.waitForByte(URC_IDENTIFIER_START_CHARACTER, 100) !=
           SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT) {

        while (SequansController.readByte() != URC_IDENTIFIER_END_CHARACTER) {}

        // Just to safe guard ourselves, wait until we actually have data
        while (!SequansController.isRxReady()) {}
        char character = SequansController.readByte();

        while (character != '\r') {
            buffer[index++] = character;

            if (index == sizeof(buffer) - 1) {
                break;
            }

            // Just to safe guard ourselves, wait until we actually have data
            while (!SequansController.isRxReady()) {}
            character = SequansController.readByte();
        }

        // NUll terminate
        buffer[index] = '\0';

        // Now we got one entry in our buffer
        // The +1 here is since the buffer will be from ':' and onwards (since
        // we are looking for the URC_IDENTIFIER_END_CHARACTER initially),
        // including the space character which we want to remove
        if (strncmp(buffer + 1, id, strlen(id))) {
            // ID did not match
            index = 0;
            continue;
        }

        // If there is a match in ID, we return the operator name
        // We subtract 4 here since we want to remove space, ',"' and '"' from
        // the length
        uint8_t operator_name_length = strlen(buffer) - strlen(id) - 4;

        // The content in our buffer will now be:
        // <space>"<id>","<name>"
        // + 1 to bypass space
        // + 2 to bypass ,"
        strncpy(buffer, buffer + 1 + strlen(id) + 2, operator_name_length);

        buffer[operator_name_length] = '\0';

        // Clear the rest of the result from querying the operators
        while (
            SequansController.waitForByte(URC_IDENTIFIER_END_CHARACTER, 100) !=
            SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT) {}

        SequansController.clearReceiveBuffer();

        return String(buffer);
    }

    SequansController.clearReceiveBuffer();
    return OPERATOR_NOT_AVAILABLE;
}

void LteClass::onConnectionStatusChange(void (*connect_callback)(void),
                                        void (*disconnect_callback)(void)) {
    connected_callback = connect_callback;
    disconnected_callback = disconnect_callback;
}

bool LteClass::isConnected(void) { return is_connected; }

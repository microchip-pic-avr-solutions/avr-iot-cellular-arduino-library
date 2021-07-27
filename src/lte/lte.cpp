#include "lte.h"
#include "sequans_controller.h"

#define AT_COMMAND_CONNECT           "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT        "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS "AT+CREG?"
#define AT_COMMAND_DISABLE_CEREG_URC "AT+CEREG=0"
#define AT_COMMAND_ENABLE_CEREG_URC  "AT+CEREG=2"
#define AT_COMMAND_DISABLE_CREG_URC  "AT+CREG=0"

#define CEREG_CALLBACK "CEREG"

// This includes null termination
#define STAT_LENGTH                  2
#define STAT_INDEX                   1
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

#define RESPONSE_CONNECTION_STATUS_SIZE 24

static bool writeCommandWithShortResponse(const char *command) {
    while (SequansController.isRxReady()) { SequansController.flushResponse(); }

    SequansController.writeCommand(command);
    return (SequansController.flushResponse() == OK);
}

void LTEClass::begin(void) {
    SequansController.begin();

    // Since we want the default to be to handle URC synchronously, we disable
    // this as they are the only ones arriving at an irregular interval
    //
    // This might fail the first times after initializing the sequans
    // controller, so we just retry until they succeed
    while (!writeCommandWithShortResponse(AT_COMMAND_DISABLE_CEREG_URC)) {}
    while (!writeCommandWithShortResponse(AT_COMMAND_DISABLE_CREG_URC)) {}

    writeCommandWithShortResponse(AT_COMMAND_CONNECT);
}

void LTEClass::end(void) {
    writeCommandWithShortResponse(AT_COMMAND_DISCONNECT);

    SequansController.end();
}

void LTEClass::registerConnectionNotificationCallback(void (*callback)()) {
    writeCommandWithShortResponse(AT_COMMAND_ENABLE_CEREG_URC);
    SequansController.registerCallback(CEREG_CALLBACK, callback);
}

bool LTEClass::isConnectedToOperator(void) {

    while (SequansController.isRxReady()) { SequansController.flushResponse(); }
    SequansController.writeCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE];

    if (SequansController.readResponse(response,
                                       RESPONSE_CONNECTION_STATUS_SIZE) != OK) {

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

#include "lte_client.h"
#include "sequans_controller.h"

#define AT_COMMAND_CONNECT           "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT        "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS "AT+CEREG?"
#define AT_COMMAND_DISABLE_CEREG_URC "AT+CEREG=0"
#define AT_COMMAND_DISABLE_CERG_URC  "AT+CERG=0"

#define STAT_INDEX                   1
#define STAT_LENGTH                  2
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

#define RESPONSE_CONNECTION_STATUS_SIZE 48

static bool writeCommandWithShortResponse(const char *command) {
    sequansControllerWriteCommand(command);
    return (sequansControllerFlushResponse() == OK);
}

void lteClientBegin(void) {
    sequansControllerBegin();

    // Since we want to handle URC synchronously, we disable this as they are
    // the only ones arriving at an irregular interval
    writeCommandWithShortResponse(AT_COMMAND_DISABLE_CEREG_URC);
    writeCommandWithShortResponse(AT_COMMAND_DISABLE_CERG_URC);
}

void lteClientEnd(void) { sequansControllerEnd(); }

bool lteClientRequestConnectionToOperator(void) {
    return writeCommandWithShortResponse(AT_COMMAND_CONNECT);
}

bool lteClientDisconnectFromOperator(void) {
    return writeCommandWithShortResponse(AT_COMMAND_DISCONNECT);
}

bool lteClientIsConnectedToOperator(void) {

    // TODO: necessary?
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }
    sequansControllerWriteCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE];
    if (sequansControllerReadResponse(response,
                                      RESPONSE_CONNECTION_STATUS_SIZE) != OK) {
        return false;
    }

    // Find the stat token in the response
    char stat_token[STAT_LENGTH];
    bool found_token = sequansControllerExtractValueFromCommandResponse(
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

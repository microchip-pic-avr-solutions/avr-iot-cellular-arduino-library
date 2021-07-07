#include "lte_client.h"
#include "sequans_controller.h"

#define AT_COMMAND_CONNECT           "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT        "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS "AT+CEREG?"

#define STAT_INDEX  1
#define STAT_LENGTH 2

#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING      '5'

#define RESPONSE_CONNECTION_STATUS_SIZE 48

static bool writeCommandWithShortResponse(const char *command) {
    sequansControllerWriteCommand(command);
    return (sequansControllerFlushResponse() == OK);
}

void lteClientBegin(void) { sequansControllerBegin(); }

void lteClientEnd(void) { sequansControllerEnd(); }

bool lteClientRequestConnectionToOperator(void) {
    return writeCommandWithShortResponse(AT_COMMAND_CONNECT);
}

bool lteClientDisconnectFromOperator(void) {
    return writeCommandWithShortResponse(AT_COMMAND_DISCONNECT);
}

bool lteClientIsConnectedToOperator(void) {

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

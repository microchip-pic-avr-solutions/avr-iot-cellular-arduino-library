#include "lte_client.h"
#include "sequans_controller.h"

#define AT_COMMAND_ENABLE_ROAMING "AT!=\"EMM::SetRoamingSupport 0\""
#define AT_COMMAND_CONNECT "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS "AT+CEREG?"

#define STAT_INDEX 1
#define STAT_LENGTH 2
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING '5'

/**
 * For responses which consist of only an "OK" or "ERROR".
 */
#define RESPONSE_DEFAULT_SIZE 16
#define RESPONSE_CONNECTION_STATUS_SIZE 48

void lteClientInitialize(void) { sequansControllerInitialize(); }

void lteClientEnableRoaming(void) {
    uint8_t result;
    char buffer[RESPONSE_DEFAULT_SIZE];
    do {
        sequansControllerSendCommand(AT_COMMAND_ENABLE_ROAMING);
        result = sequansControllerFlushResponse();
    } while (result != SEQUANS_CONTROLLER_RESPONSE_OK);
}

void lteClientConnectToOperator(void) {
    uint8_t result;
    char buffer[RESPONSE_DEFAULT_SIZE];
    do {
        sequansControllerSendCommand(AT_COMMAND_CONNECT);
        result = sequansControllerFlushResponse();
    } while (result != SEQUANS_CONTROLLER_RESPONSE_OK);
}

bool lteClientIsConnectedToOperator(void) {
    sequansControllerSendCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE];
    sequansControllerReadResponse(response, RESPONSE_CONNECTION_STATUS_SIZE);

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

void lteClientDisconnectFromOperator(void) {
    sequansControllerSendCommand(AT_COMMAND_DISCONNECT);
}

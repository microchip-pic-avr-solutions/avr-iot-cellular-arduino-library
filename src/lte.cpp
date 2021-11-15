#include "lte.h"
#include "sequans_controller.h"
#include <Arduino.h>

#define AT_COMMAND_CONNECT "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS "AT+CEREG?"
#define AT_COMMAND_DISABLE_CEREG_URC "AT+CEREG=0"
#define AT_COMMAND_ENABLE_CEREG_URC "AT+CEREG=2"
#define AT_COMMAND_DISABLE_CREG_URC "AT+CREG=0"

#define CEREG_CALLBACK "CEREG"

// This includes null termination
#define STAT_LENGTH 2
#define STAT_INDEX 1
#define STAT_REGISTERED_HOME_NETWORK '1'
#define STAT_REGISTERED_ROAMING '5'

#define RESPONSE_CONNECTION_STATUS_SIZE 24

#define CEREG_DATA_LENGTH 2

#define CEREG_N_OK 2

// When the CEREG appears as an URC, it only includes the stat, but there will
// be a space before the data, hence this value since this index is character
// index.
#define CEREG_STAT_CHARACTER_INDEX 1

// Singleton. Defined for use of the rest of the library.
LteClass Lte = LteClass::instance();

static void (*connected_callback)(void) = NULL;
static void (*disconnected_callback)(void) = NULL;

static void connectionStatus(void)
{
    // +1 for null termination
    char buffer[CEREG_DATA_LENGTH + 1];

    if (SequansController.readNotification(buffer, sizeof(buffer)))
    {

        const char stat = buffer[CEREG_STAT_CHARACTER_INDEX];

        if (stat == STAT_REGISTERED_ROAMING ||
            stat == STAT_REGISTERED_HOME_NETWORK)
        {

            if (connected_callback)
            {
                connected_callback();
            }
        }
        else
        {
            if (disconnected_callback)
            {
                disconnected_callback();
            }
        }
    }
}

void LteClass::begin(void)
{
    SequansController.begin();

    SequansController.clearReceiveBuffer();

    // This might fail the first times after initializing the sequans
    // controller, so we just retry until they succeed
    SequansController.retryCommand(AT_COMMAND_DISABLE_CREG_URC);
    SequansController.retryCommand(AT_COMMAND_ENABLE_CEREG_URC);
    SequansController.retryCommand(AT_COMMAND_CONNECT);

    // This is convenient when the MCU has been issued a reset, but the lte
    // modem is alerady connected, which will be the case during development for
    // example. In that way, the user gets the callback upon start and doesn't
    // have to check themselves
    if (isConnected() && connected_callback != NULL)
    {
        connected_callback();
    }
}

void LteClass::end(void)
{
    SequansController.retryCommand(AT_COMMAND_DISCONNECT);
    SequansController.end();
}

void LteClass::onConnectionStatusChange(void (*connect_callback)(void),
                                        void (*disconnect_callback)(void))
{
    connected_callback = connect_callback;
    disconnected_callback = disconnect_callback;
    SequansController.registerCallback(CEREG_CALLBACK, connectionStatus);
}

bool LteClass::isConnected(void)
{

    SequansController.clearReceiveBuffer();
    SequansController.writeCommand(AT_COMMAND_CONNECTION_STATUS);

    char response[RESPONSE_CONNECTION_STATUS_SIZE];

    ResponseResult res = SequansController.readResponse(response,
                                                        RESPONSE_CONNECTION_STATUS_SIZE);
    if (res != CEREG_N_OK)
    {
        return false;
    }

    // Find the stat token in the response
    char stat_token[STAT_LENGTH];
    bool found_token = SequansController.extractValueFromCommandResponse(
        response, STAT_INDEX, stat_token, STAT_LENGTH);

    if (!found_token)
    {
        return false;
    }

    if (stat_token[0] == STAT_REGISTERED_HOME_NETWORK ||
        stat_token[0] == STAT_REGISTERED_ROAMING)
    {
        return true;
    }

    return false;
}

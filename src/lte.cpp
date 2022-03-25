#include "led_ctrl.h"
#include "log.h"
#include "lte.h"
#include "mqtt_client.h"
#include "sequans_controller.h"

#include <Arduino.h>

#define AT_COMMAND_CONNECT           "AT+CFUN=1"
#define AT_COMMAND_DISCONNECT        "AT+CFUN=0"
#define AT_COMMAND_CONNECTION_STATUS "AT+CEREG?"
#define AT_COMMAND_DISABLE_CEREG_URC "AT+CEREG=0"
#define AT_COMMAND_ENABLE_CEREG_URC  "AT+CEREG=5"
#define AT_COMMAND_DISABLE_CREG_URC  "AT+CREG=0"

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

void LteClass::begin(void) {

    // If low power is utilized, sequans controller will already been
    // initialized, so don't reset it by calling begin again
    if (!SequansController.isInitialized()) {
        SequansController.begin();

        // Allow 500ms for boot
        delay(500);
    }

    SequansController.clearReceiveBuffer();

    // This might fail the first times after initializing the sequans
    // controller, so we just retry until they succeed
    SequansController.retryCommand(AT_COMMAND_DISABLE_CREG_URC);
    SequansController.retryCommand(AT_COMMAND_ENABLE_CEREG_URC);
    SequansController.retryCommand(AT_COMMAND_CONNECT);

    // Enable the default callback
    SequansController.registerCallback(CEREG_CALLBACK, connectionStatus);

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
}

bool LteClass::isConnected(void) { return is_connected; }

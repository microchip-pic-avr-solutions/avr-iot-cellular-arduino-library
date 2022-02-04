#include <Arduino.h>
#include <log.h>
#include <lte.h>
#include <sequans_controller.h>

#define SerialDebug Serial5

#define RTS_PIN    PIN_PC7
#define RTS_PORT   PORTC
#define RTS_PIN_bm PIN7_bm

void power_save_abrupted(void) { Log.info("Power save abrupted"); }

void setup() {

    Serial5.begin(115200);
    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    if (!Lte.configurePowerSaveMode(SleepUnitMultiplier::ONE_MINUTE,
                                    1,
                                    AwakeUnitMultiplier::ONE_MINUTE,
                                    1)) {

        Log.warn("Not able to configure power save mode\r\n");
        return;
    }

    Lte.onPowerSaveAbrupted(power_save_abrupted);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
    while (!Lte.isConnected()) {
        Log.info("Not connected yet...\r\n");
        delay(1000);
    }

    Log.info("Connected!\r\n");
}

void loop() { debugBridgeUpdate(); }

// ------------------------------ DEBUG BRIDGE ----------------------------- //

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    128
#define RESPONSE_BUFFER_SIZE 256

void debugBridgeUpdate(void) {
    static uint8_t character;
    static char input_buffer[INPUT_BUFFER_SIZE];
    static uint8_t input_buffer_index = 0;

    if (Serial5.available() > 0) {
        character = Serial5.read();

        switch (character) {
        case DEL_CHARACTER:
            if (strlen(input_buffer) > 0) {
                input_buffer[input_buffer_index--] = 0;
            }
            break;

        case ENTER_CHARACTER:

            if (strstr(input_buffer, "pw_enable")) {
                Log.info("Attempting to enter power save mode. "
                         "Waiting for success...\r\n");
                Log.infof("Success: %d\r\n", Lte.attemptToEnterPowerSaveMode());

            } else if (strstr(input_buffer, "pw_disable")) {
                Log.info("Ending power save\r\n");
                Lte.endPowerSaveMode();
            } else {
                SequansController.writeCommand(input_buffer);
            }

            // Reset buffer
            memset(input_buffer, 0, sizeof(input_buffer));
            input_buffer_index = 0;

            break;

        default:
            input_buffer[input_buffer_index++] = character;
            break;
        }

        Serial5.print((char)character);
    }

    if (SequansController.isRxReady()) {
        // Send back data from modem to host
        Serial5.write(SequansController.readByte());
    }
}

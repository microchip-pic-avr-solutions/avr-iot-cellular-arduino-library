#include <Arduino.h>
#include <lte.h>
#include <sequans_controller.h>

#define SerialDebug Serial5

#define RTS_PIN    PIN_PC7
#define RTS_PORT   PORTC
#define RTS_PIN_bm PIN7_bm

void setup() {

    SerialDebug.begin(115200);

    // Start LTE modem and wait until we are connected to the operator
    /*Lte.begin();
    while (!Lte.isConnected()) {
        Serial5.println("Not connected to operator yet...");
        delay(1000);
    }

    Serial5.println("Connected!");
    */

    SequansController.begin();
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
                SequansController.setPowerSaveMode(1, NULL);
                Serial5.println("Enabling low power, pulling rts high");
            } else if (strstr(input_buffer, "pw_disable")) {
                SequansController.setPowerSaveMode(0, NULL);
                Serial5.println("Disabling low power, pulling rts low");
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

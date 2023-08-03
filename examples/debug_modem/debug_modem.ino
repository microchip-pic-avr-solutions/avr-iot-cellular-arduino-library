#include <Arduino.h>
#include <log.h>
#include <sequans_controller.h>

void debugBridgeUpdate(void);

void setup() {
    Log.begin(115200);
    SequansController.begin();
}

void loop() { debugBridgeUpdate(); }

// ------------------------------ DEBUG BRIDGE ----------------------------- //

#define SerialDebug Serial3

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    256
#define RESPONSE_BUFFER_SIZE 256

void debugBridgeUpdate(void) {
    static uint8_t character;
    static char input_buffer[INPUT_BUFFER_SIZE];
    static uint8_t input_buffer_index = 0;

    if (SerialDebug.available() > 0) {
        character = SerialDebug.read();

        switch (character) {
        case DEL_CHARACTER:
            if (strlen(input_buffer) > 0) {
                input_buffer[input_buffer_index--] = 0;
            }
            break;

        case ENTER_CHARACTER:
            input_buffer[input_buffer_index]     = '\r';
            input_buffer[input_buffer_index + 1] = '\0';
            SequansController.writeBytes((const uint8_t*)input_buffer,
                                         strlen(input_buffer));

            // Reset buffer
            memset(input_buffer, 0, sizeof(input_buffer));
            input_buffer_index = 0;

            break;

        default:
            input_buffer[input_buffer_index++] = character;
            break;
        }

        SerialDebug.print((char)character);
    }

    if (SequansController.isRxReady()) {
        // Send back data from modem to host
        SerialDebug.write(SequansController.readByte());
    }
}

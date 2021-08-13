#include <Arduino.h>
#include <http_client.h>
#include <lte.h>
#include <sequans_controller.h>

#define DOMAIN "httpbin.org"

#define SerialDebug Serial5

void testHttp();

void setup() {

    SerialDebug.begin(115200);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
    while (!Lte.isConnected()) {
        Serial5.println("Not connected to operator yet...");
        delay(1000);
    }

    Serial5.println("Connected!");

    testHttp();
}

void testHttp() {

    SerialDebug.println("---- Testing HTTP ----");

    HttpResponse response;

    // --- HTTP ---

    if (!HttpClient.configure(DOMAIN, 80, false)) {
        SerialDebug.println("Failed to configure http client");
    }

    SerialDebug.println("Configured to HTTP");

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    SerialDebug.printf("POST - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    // --- HTTPS ---

    if (!HttpClient.configure(DOMAIN, 443, true)) {
        SerialDebug.println("Failed to configure https client");
    }

    SerialDebug.println("Configured to HTTPS");

    // This fails...
    response = HttpClient.head("/get");
    SerialDebug.printf("HEAD - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    response = HttpClient.get("/get");
    SerialDebug.printf("GET - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    String body = HttpClient.readBody();

    if (body != "") {
        SerialDebug.print("Body:\r\n");
        SerialDebug.print(body);
    }
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

            SequansController.writeCommand(input_buffer);

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

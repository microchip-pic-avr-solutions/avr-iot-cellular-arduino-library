#include "test.h"

#include <Arduino.h>
//#include <Wire.h>
//#include <cryptoauthlib.h>
#include <stdio.h>

//#include "src/http_client.h"
#include "src/sequans_controller.h"

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    128
#define RESPONSE_BUFFER_SIZE 256

/*
void testTwi(void) {
    Wire1.begin();

    Serial5.println("Starting scan...");

    for (uint8_t i = 1; i < 128; i++) {
        Wire1.beginTransmission(i);
        Wire1.write(0x99);
        Wire1.endTransmission();

        if (!(TWI1.MSTATUS & (1 << 4))) {
            Serial5.printf("Got ack from %X\r\n", i);
        }
    }

    Wire1.end();
}

void testHttp() {

    Serial5.println("---- Testing HTTP ----");
    HttpResponse response;

    // --- HTTP ---
    HttpClient http_client;

    if (!http_client.configure("www.ptsv2.com", 80, false)) {
        Serial5.println("Failed to configure http client");
    }

    Serial5.println("Configured to HTTP");

    const char *payload = "{\"hellothere\": \"generalkenobi\"}";
    response =
        http_client.post("/t/1rqc3-1624431962/post", payload, strlen(payload));

    Serial5.printf("POST - status code: %d, data size: %d\r\n",
                   response.status_code,
                   response.data_size);

    // --- HTTPS ---

    if (!http_client.configure("raw.githubusercontent.com", 443, true)) {
        Serial5.println("Failed to configure http client");
    }

    Serial5.println("Configured to HTTPS");

    response = http_client.head("/SpenceKonde/DxCore/master/.gitignore");

    Serial5.printf("HEAD - status code: %d, data size: %d\r\n",
                   response.status_code,
                   response.data_size);

    response = http_client.get("/SpenceKonde/DxCore/master/.gitignore");
    Serial5.printf("GET - status code: %d, data size: %d\r\n",
                   response.status_code,
                   response.data_size);

    // Add NULL termination
    char response_buffer[response.data_size + 1] = "";
    uint16_t bytes_read = http_client.readBody(response_buffer, 64);

    if (bytes_read > 0) {

        Serial5.printf("Retrieving data completed successfully. Bytes read %d, "
                       "body\r\n %s\r\n",
                       bytes_read,
                       response_buffer);
    }
}

*/
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

            /*
            if (memcmp(input_buffer, "http", 4) == 0) {
                testHttp();
            } else if (memcmp(input_buffer, "twi", 3) == 0) {
                testTwi();
            } else {
            */
            SequansController.writeCommand(input_buffer);
            //}

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

#include "http_client.h"
#include "lte_client.h"
#include "sequans_controller.h"

#include <string.h>

#define INPUT_BUFFER_SIZE 128
#define RESPONSE_BUFFER_SIZE 256

#define DEL_CHARACTER 127
#define ENTER_CHARACTER 13

static bool connected = false;
static bool check_connection = false;
static bool tested_http = false;

void setupConnectionStatusTimer(void) {
    takeOverTCA0();

    TCA0.SINGLE.PER = 0xFFFF;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
}

ISR(TCA0_OVF_vect) {
    check_connection = true;

    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

/**
 * @brief Relay bridge for AT commands and responses.
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
            sequansControllerSendCommand(input_buffer);

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

    if (sequansControllerIsRxReady()) {
        // Send back data from modem to host
        Serial5.write(sequansControllerReadByte());
    }
}

void testHttp() {
    httpClientConfigure("ptsv2.com", 80);
    HttpResponse response =
        httpClientPost("/t/1rqc3-1624431962/post", "{\"hello\": \"world\"}");
    Serial5.print("POST completed with status code ");
    Serial5.print(response.status_code);
    Serial5.print(" and data size ");
    Serial5.println(response.data_size);

    httpClientConfigure("google.com", 80);
    response = httpClientHead("/");

    Serial5.print("HEAD request to google.com. Status code is ");
    Serial5.print(response.status_code);
    Serial5.print(" and data size is ");
    Serial5.println(response.data_size);

    response = httpClientGet("/");

    if (response.status_code != 404 && response.status_code != 0) {
        // Add NULL termination
        char response_buffer[response.data_size + 1] = "";
        uint8_t success = httpClientReadResponseBody(response_buffer,
                                                     response.data_size + 32);

        if (success) {

            Serial5.print("GET request to google.com. Body is: ");
            Serial5.println(response_buffer);
        }
    }
}

void setup() {
    Serial5.begin(115200);

    setupConnectionStatusTimer();

    lteClientInitialize();
    lteClientEnableRoaming();
    lteClientConnectToOperator();

    Serial5.println("---- Finished initializing ----");
}

void loop() {

    while (1) {
        debugBridgeUpdate();

        if (check_connection) {

            bool new_connection_status = lteClientIsConnectedToOperator();

            if (new_connection_status != connected) {
                connected = new_connection_status;
                Serial5.print("New connection status, connected: ");
                Serial5.println(connected);
            }

            if (connected && !tested_http) {
                testHttp();
                tested_http = true;
            }

            check_connection = false;
        }
    }
}

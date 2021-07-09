#include "src/ecc/ecc_controller.h"
#include "src/lte/http_client.h"
#include "src/lte/lte_client.h"
#include "src/lte/mqtt_client.h"
#include "src/lte/sequans_controller.h"

#include "cryptoauthlib.h"
#include <Arduino.h>
#include <string.h>

#define CELL_STATUS_LED PIN_PG2

#define INPUT_BUFFER_SIZE    128
#define RESPONSE_BUFFER_SIZE 256

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

static bool connected = false;
static bool check_connection = false;

void testHttp();
void testMqtt();
void testECC();

ISR(TCA0_OVF_vect) {
    check_connection = true;

    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

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

            if (memcmp(input_buffer, "http", 4) == 0) {
                testHttp();
            } else if (memcmp(input_buffer, "mqtt", 4) == 0) {
                testMqtt();
            } else if (memcmp(input_buffer, "ecc", 3) == 0) {
                testECC();
            } else {
                sequansControllerWriteCommand(input_buffer);
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

    if (sequansControllerIsRxReady()) {
        // Send back data from modem to host
        Serial5.write(sequansControllerReadByte());
    }
}

void testHttp() {

    Serial5.println("---- Testing HTTP ----");
    HttpResponse response;

    // --- HTTP ---

    while (!httpClientConfigure("www.ptsv2.com", 80, false)) {}

    Serial5.println("Configured to HTTP");

    const char *payload = "{\"hellothere\": \"generalkenobi\"}";
    response =
        httpClientPost("/t/1rqc3-1624431962/post", payload, strlen(payload));

    Serial5.printf("POST - status code: %d, data size: %d\r\n",
                   response.status_code,
                   response.data_size);

    // --- HTTPS ---

    while (!httpClientConfigure("raw.githubusercontent.com", 443, true)) {}

    Serial5.println("Configured to HTTPS");

    response = httpClientHead("/SpenceKonde/DxCore/master/.gitignore");

    Serial5.printf("HEAD - status code: %d, data size: %d\r\n",
                   response.status_code,
                   response.data_size);

    response = httpClientGet("/SpenceKonde/DxCore/master/.gitignore");
    Serial5.printf("GET - status code: %d, data size: %d\r\n",
                   response.status_code,
                   response.data_size);

    // Add NULL termination
    char response_buffer[response.data_size + 1] = "";
    uint16_t bytes_read = httpClientReadResponseBody(response_buffer, 64);

    if (bytes_read > 0) {

        Serial5.printf("Retrieving data completed successfully. Bytes read %d, "
                       "body\r\n %s\r\n",
                       bytes_read,
                       response_buffer);
    }
}

void testMqtt() {

    Serial5.println("---- Testing MQTT ----");
    if (!mqttClientConfigure("testthingyiot", false)) {
        Serial5.println("Failed to configure MQTT");
        return;
    }

    Serial5.println("Configured MQTT");

    if (!mqttClientConnect("test.mosquitto.org", 1883)) {
        Serial5.println("Failed to connect");
        return;
    }

    Serial5.println("Connected to MQTT broker");

    const char *message = "Hello";

    if (!mqttClientPublish("iottest", 0, (uint8_t *)message, strlen(message))) {
        Serial5.println("Failed to publish");
        return;
    }

    Serial5.println("Published to MQTT broker");
}

void testECC() {
    Serial5.println("---- Testing ECC ----");
    if (!eccControllerInitialize()) {
        Serial5.println("ECC controller failed to initialize");
        return;
    }

    uint8_t serial_number[ECC_SERIAL_NUMBER_LENGTH];

    if (!eccControllerRetrieveSerialNumber((uint8_t *)serial_number)) {
        Serial5.println("ECC controller failed to retrieve serial number");
        return;
    }

    Serial5.print("Serial number: ");
    for (size_t i = 0; i < sizeof(serial_number); i++) {
        Serial5.print(serial_number[i], HEX);
    }
    // Should be: 01 23 43 6B DB 97 28 E5 01

    Serial5.println();

    uint8_t public_key[64];

    if (!eccControllerRetrievePublicKey(0, &public_key[0])) {
        return;
    }

    Serial5.print("Public key: ");
    for (size_t i = 0; i < sizeof(public_key); i++) {
        Serial5.print(public_key[i], HEX);
    }

    Serial5.println();

    uint8_t message[ECC_SIGN_MESSSAGE_LENGTH];
    sprintf(message, "Hello sign");
    uint8_t signature[ECC_SIGN_MESSSAGE_LENGTH];

    if (!eccControllerSignMessage(0, &message[0], &signature[0])) {
        return;
    }

    Serial5.print("Signed message: ");

    for (size_t i = 0; i < sizeof(signature); i++) {
        Serial5.print(signature[i], HEX);
    }

    Serial5.println();
}

void setupConnectionStatusTimer(void) {
    takeOverTCA0();

    TCA0.SINGLE.PER = 0xFFFF;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
}

void setup() {
    Serial5.begin(115200);

    Serial5.println("---- Starting initializing ----");

    // Pin is active low
    pinMode(CELL_STATUS_LED, OUTPUT);
    digitalWrite(CELL_STATUS_LED, HIGH);

    setupConnectionStatusTimer();
    lteClientBegin();

    while (!lteClientRequestConnectionToOperator()) {}

    Serial5.println("---- Finished initializing ----");
}

void loop() {

    debugBridgeUpdate();

    if (check_connection) {

        bool new_connection_status = lteClientIsConnectedToOperator();

        if (new_connection_status != connected) {
            connected = new_connection_status;

            // Pin is active low
            digitalWrite(CELL_STATUS_LED, connected ? LOW : HIGH);
        }

        check_connection = false;
    }
}

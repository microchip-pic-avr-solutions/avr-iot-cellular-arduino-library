#include "test.h"

#include <Arduino.h>
#include <stdio.h>

#include "src/http_client.h"
#include "src/lte.h"

#define SerialInterface SerialInterface

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3
#define DATA_LED       PIN_PG4
#define ERROR_LED      PIN_PG5

void setupPins(void) {
    // These pins is active low
    pinMode(CELL_LED, OUTPUT);
    pinMode(CONNECTION_LED, OUTPUT);
    pinMode(DATA_LED, OUTPUT);
    pinMode(ERROR_LED, OUTPUT);

    digitalWrite(CELL_LED, HIGH);
    digitalWrite(CONNECTION_LED, HIGH);
    digitalWrite(DATA_LED, HIGH);
    digitalWrite(ERROR_LED, HIGH);
}

void connectedToNetwork(void) { digitalWrite(CELL_LED, LOW); }

void disconnectedFromNetwork(void) { digitalWrite(CELL_LED, HIGH); }

void setup() {

    setupPins();

    SerialInterface.begin(115200);

    Lte.onConnectionStatusChange(connectedToNetwork, disconnectedFromNetwork);
    LTE.begin();

    if (Lte.isConnected()) {
        digitalWrite(CELL_LED, LOW);
    }
}

static bool tested_http = false;

void testHttp() {

    SerialInterface.println("---- Testing HTTP ----");
    HttpResponse response;

    // --- HTTP ---
    HttpClient http_client;

    if (!http_client.configure("www.ptsv2.com", 80, false)) {
        SerialInterface.println("Failed to configure http client");
    }

    SerialInterface.println("Configured to HTTP");

    const char *payload = "{\"hellothere\": \"generalkenobi\"}";
    response =
        http_client.post("/t/1rqc3-1624431962/post", payload, strlen(payload));

    SerialInterface.printf("POST - status code: %d, data size: %d\r\n",
                           response.status_code,
                           response.data_size);

    // --- HTTPS ---

    if (!http_client.configure("raw.githubusercontent.com", 443, true)) {
        SerialInterface.println("Failed to configure http client");
    }

    SerialInterface.println("Configured to HTTPS");

    response = http_client.head("/SpenceKonde/DxCore/master/.gitignore");

    SerialInterface.printf("HEAD - status code: %d, data size: %d\r\n",
                           response.status_code,
                           response.data_size);

    response = http_client.get("/SpenceKonde/DxCore/master/.gitignore");
    SerialInterface.printf("GET - status code: %d, data size: %d\r\n",
                           response.status_code,
                           response.data_size);

    // Add NULL termination
    char response_buffer[response.data_size + 1] = "";
    uint16_t bytes_read = http_client.readBody(response_buffer, 64);

    if (bytes_read > 0) {

        SerialInterface.printf(
            "Retrieving data completed successfully. Bytes read %d, "
            "body\r\n %s\r\n",
            bytes_read,
            response_buffer);
    }
}

void loop() {

    // Polling for LTE network connection is also possible
    if (LTE.isConnected() && !tested_http) {
        testHttp();
    }

    delay(1000);
}

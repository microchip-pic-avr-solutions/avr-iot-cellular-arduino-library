#include <Arduino.h>
#include <http_client.h>
#include <lte.h>

#define HTTP_DOMAIN   "www.ptsv2.com"
#define HTTP_ENDPOINT "/t/1rqc3-1624431962/post"

#define HTTPS_DOMAIN   "raw.githubusercontent.com"
#define HTTPS_ENDPOINT "/SpenceKonde/DxCore/master/.gitignore"

#define SerialDebug Serial5

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3
#define DATA_LED       PIN_PG4
#define ERROR_LED      PIN_PG5

static bool tested_http = false;
static bool connected_to_network = false;

void connectedToNetwork(void) {
    connected_to_network = true;
    digitalWrite(CELL_LED, LOW);
}

void disconnectedFromNetwork(void) {
    connected_to_network = false;
    digitalWrite(CELL_LED, HIGH);
}

void setup() {
    pinMode(CELL_LED, OUTPUT);
    pinMode(CONNECTION_LED, OUTPUT);
    pinMode(DATA_LED, OUTPUT);
    pinMode(ERROR_LED, OUTPUT);

    // These pins is active low
    digitalWrite(CELL_LED, HIGH);
    digitalWrite(CONNECTION_LED, HIGH);
    digitalWrite(DATA_LED, HIGH);
    digitalWrite(ERROR_LED, HIGH);

    SerialDebug.begin(115200);

    // Register callbacks for network connection
    Lte.onConnectionStatusChange(connectedToNetwork, disconnectedFromNetwork);
    Lte.begin();
}

void testHttp() {

    SerialDebug.println("---- Testing HTTP ----");

    HttpResponse response;

    // --- HTTP ---

    if (!HttpClient.configure(HTTPS_DOMAIN, 80, false)) {
        SerialDebug.println("Failed to configure http client");
    }

    SerialDebug.println("Configured to HTTP");

    response = HttpClient.post(HTTPS_ENDPOINT, "{\"hello\": \"there\"}");
    SerialDebug.printf("POST - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    // --- HTTPS ---

    if (!HttpClient.configure(HTTPS_DOMAIN, 443, true)) {
        SerialDebug.println("Failed to configure https client");
    }

    SerialDebug.println("Configured to HTTPS");

    response = HttpClient.head(HTTPS_ENDPOINT);
    SerialDebug.printf("HEAD - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    response = HttpClient.get("/SpenceKonde/DxCore/master/.gitignore");
    SerialDebug.printf("GET - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    // Add min amount and NULL termination
    // TODO: test this more and explain it
    char response_buffer[response.data_size + 64 + 1] = "";
    uint16_t bytes_read =
        HttpClient.readBody(response_buffer, sizeof(response_buffer));

    if (bytes_read > 0) {

        SerialDebug.printf(
            "Retrieving data completed successfully. Bytes read %d, "
            "body\r\n %s\r\n",
            bytes_read,
            response_buffer);
    }
}

void loop() {

    if (connected_to_network && !tested_http) {
        testHttp();
    }

    delay(1000);
}

#include <Arduino.h>
#include <http_client.h>
#include <lte.h>

#define HTTP_DOMAIN   "www.ptsv2.com"
#define HTTP_ENDPOINT "/t/1rqc3-1624431962/post"

#define HTTPS_DOMAIN   "raw.githubusercontent.com"
#define HTTPS_ENDPOINT "/SpenceKonde/DxCore/master/.gitignore"

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

    if (!HttpClient.configure(HTTP_DOMAIN, 80, false)) {
        SerialDebug.println("Failed to configure http client");
    }

    SerialDebug.println("Configured to HTTP");

    response = HttpClient.post(HTTP_ENDPOINT, "{\"hello\": \"world\"}");
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

    response = HttpClient.get(HTTPS_ENDPOINT);
    SerialDebug.printf("GET - status code: %d, data size: %d\r\n",
                       response.status_code,
                       response.data_size);

    // TODO: test this with over 128 bytes, and write something about it
    String body = HttpClient.readBody();

    if (body != "") {
        SerialDebug.printf("Body:\r\n %s\r\n", body);
    }
}

void loop() {}

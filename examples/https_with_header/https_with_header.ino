/**
 * @brief This example demonstrates HTTPS GET request with appended
 * headers. Adding header also works for the other methods such as POST, PUT,
 * HEAD and DELETE as well as for plain HTTP without TLS.
 */
#include <Arduino.h>
#include <http_client.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>

#define DOMAIN "httpbin.org"

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.begin(115200);
    Log.info(F("Starting HTTPS with header example"));

    // Start LTE modem and connect to the operator
    if (!Lte.begin()) {
        Log.error(F("Failed to connect to the operator"));
        return;
    }

    Log.infof(F("Connected to operator: %s\r\n"), Lte.getOperator().c_str());

    Log.info(F("Performing GET with header..."));

    // For HTTP without TLS, use: HttpClient.configure(DOMAIN, 80, false)
    if (!HttpClient.configure(DOMAIN, 443, true)) {
        Log.info(F("Failed to configure https client\r\n"));
        return;
    }

    // For other methods such as POST, use:
    // HttpClient.post("<endpoint>", "<data>", "Authorization: Bearer");
    HttpResponse response = HttpClient.get("/get", "Authorization: Bearer");

    Log.infof(F("GET - HTTP status code: %u, data size: %u\r\n"),
              response.status_code,
              response.data_size);

    // Add some extra bytes for termination
    String response_data = HttpClient.readBody();

    if (response_data != "") {
        Log.infof(F("Response: %s\r\n"), response_data.c_str());
    }
}

void loop() {}

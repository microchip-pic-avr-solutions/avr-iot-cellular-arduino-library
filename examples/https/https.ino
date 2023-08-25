/**
 * @brief This example demonstrates HTTPS GET and POST request as well as
 * reading out the the response of the POST message.
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
    Log.info(F("Starting HTTPS example"));

    // Start LTE modem and connect to the operator
    if (!Lte.begin()) {
        Log.error(F("Failed to connect to the operator"));
        return;
    }

    Log.infof(F("Connected to operator: %s\r\n"), Lte.getOperator().c_str());

    // --- HTTPS ---
    Log.info(F("---- Testing HTTPS ----"));

    if (!HttpClient.configure(DOMAIN, 443, true)) {
        Log.info(F("Failed to configure https client\r\n"));
        return;
    }

    Log.info(F("Configured to HTTPS"));

    HttpResponse response = HttpClient.get("/get");
    Log.infof(F("GET - HTTP status code: %u, data size: %u\r\n"),
              response.status_code,
              response.data_size);

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    Log.infof(F("POST - HTTP status code: %u, data size: %u\r\n"),
              response.status_code,
              response.data_size);

    // Add some extra bytes for termination
    String body = HttpClient.readBody(response.data_size + 16);

    if (body != "") {
        Log.infof(F("Body: %s\r\n"), body.c_str());
    }
}

void loop() {}

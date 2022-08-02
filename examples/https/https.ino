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

void testHttp();

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.begin(115200);
    Log.info("Starting HTTPS example");

    // Start LTE modem and connect to the operator
    if (!Lte.begin()) {
        Log.error("Failed to connect to the operator");
        return;
    }

    Log.infof("Connected to operator: %s\r\n", Lte.getOperator().c_str());

    // --- HTTPS ---
    Log.info("---- Testing HTTPS ----");

    if (!HttpClient.configure(DOMAIN, 443, true)) {
        Log.info("Failed to configure https client\r\n");
        return;
    }

    Log.info("Configured to HTTPS");

    HttpResponse response = HttpClient.get("/get");
    Log.infof("GET - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    Log.infof("POST - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    // Add some extra bytes for termination
    String body = HttpClient.readBody(response.data_size + 16);

    if (body != "") {
        Log.infof("Body: %s\r\n", body.c_str());
    }
}

void loop() {}

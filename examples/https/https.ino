#include <Arduino.h>
#include <http_client.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>

#define DOMAIN "httpbin.org"

void testHttp();

void setup() {

    Log.begin(115200);

    LedCtrl.begin();
    LedCtrl.startupCycle();

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
    }

    Log.info("Configured to HTTPS");

    HttpResponse response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    Log.infof("POST - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    String body = HttpClient.readBody(512);

    if (body != "") {
        Log.infof("Body: %s\r\n", body.c_str());
    }
}

void loop() {}

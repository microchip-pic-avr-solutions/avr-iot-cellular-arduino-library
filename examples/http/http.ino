#include <Arduino.h>
#include <http_client.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>

#define DOMAIN "httpbin.org"

void testHttp();

void setup() {

    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Start LTE modem and connect to the operator
    if (!Lte.begin()) {
        Log.error("Failed to connect to the operator");
        return;
    }

    Log.infof("Connected to operator: %s\r\n", Lte.getOperator().c_str());

    // --- HTTP ---

    Log.info("---- Testing HTTP ----");

    if (!HttpClient.configure(DOMAIN, 80, false)) {
        Log.info("Failed to configure http client\r\n");
    }

    Log.info("Configured to HTTP");

    HttpResponse response = HttpClient.get("/get");
    Log.infof("GET - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    Log.infof("POST - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    String body = HttpClient.readBody(512);

    if (body != "") {
        Log.infof("Body: %s\r\n", body.c_str());
    }
}

void loop() {}

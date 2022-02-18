/**
 * @note This example with HTTPS requires the security profile for HTTPS to be
 * set up (AT+SQNSPCFG=3,2,"",1,1)
 */
#include <Arduino.h>
#include <http_client.h>
#include <log.h>
#include <lte.h>
#include <sequans_controller.h>

#define DOMAIN "httpbin.org"

void testHttp();

void setup() {

    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
    while (!Lte.isConnected()) {
        Log.info("Not connected to operator yet...\r\n");
        delay(1000);
    }

    Log.info("Connected!\r\n");

    testHttp();
}

void testHttp() {

    Log.info("---- Testing HTTP ----\r\n");

    HttpResponse response;

    // --- HTTP ---

    if (!HttpClient.configure(DOMAIN, 80, false)) {
        Log.info("Failed to configure http client\r\n");
    }

    Log.info("Configured to HTTP\r\n");

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    Log.infof("POST - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    // --- HTTPS ---

    if (!HttpClient.configure(DOMAIN, 443, true)) {
        Log.info("Failed to configure https client\r\n");
    }

    Log.info("Configured to HTTPS\r\n");

    response = HttpClient.head("/get");
    Log.infof("HEAD - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    response = HttpClient.get("/get");
    Log.infof("GET - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    String body = HttpClient.readBody(512);

    if (body != "") {
        Log.info("Body:\r\n");
        Log.raw(body);
    }
}

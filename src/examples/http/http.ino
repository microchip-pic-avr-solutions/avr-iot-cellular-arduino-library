#include <Arduino.h>
#include <http_client.h>
#include <lte.h>
#include <sequans_controller.h>
#include "log/log.h"

#define DOMAIN "httpbin.org"

void testHttp();

void setup()
{

    LOG.begin(115200);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
    while (!Lte.isConnected())
    {
        LOG.Info("Not connected to operator yet...");
        delay(1000);
    }

    LOG.Info("Connected!");

    testHttp();
}

void testHttp()
{

    LOG.Info("---- Testing HTTP ----");

    HttpResponse response;

    // --- HTTP ---

    if (!HttpClient.configure(DOMAIN, 80, false))
    {
        LOG.Error("Failed to configure http client");
    }

    LOG.Info("Configured to HTTP");

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    LOG.Infof("POST - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    // --- HTTPS ---

    if (!HttpClient.configure(DOMAIN, 443, true))
    {
        LOG.Error("Failed to configure https client");
    }

    LOG.Info("Configured to HTTPS");

    // This fails...
    response = HttpClient.head("/get");
    LOG.Infof("HEAD - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    response = HttpClient.get("/get");
    LOG.Infof("GET - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    String body = HttpClient.readBody(512);

    if (body != "")
    {
        LOG.Info("Body:\r\n");
        LOG.Info(body);
    }
}

void loop() {}
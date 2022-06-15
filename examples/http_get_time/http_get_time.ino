#include <Arduino.h>
#include <http_client.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>

#define TIMEZONE_URL "worldtimeapi.org"
#define TIMEZONE_URI "/api/timezone/Europe/Oslo.txt"

long getTimeFromApiresp(String *resp) {
    int unixTimeIndex = resp->indexOf(String("unixtime: "));
    int utx_datetimeIndex = resp->indexOf(String("utc_datetime"));

    String substr = resp->substring(unixTimeIndex + 10, utx_datetimeIndex - 1);
    return substr.toInt();
}

void setup() {

    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.begin(115200);
    Log.info("Starting HTTP Get Time Example\r\n");

    // Start LTE modem and connect to the operator
    if (!Lte.begin()) {
        Log.info("Failed to connect to operator");
        return;
    }

    Log.infof("Connected to operator: %s\r\n", Lte.getOperator().c_str());

    if (!HttpClient.configure(TIMEZONE_URL, 80, false)) {
        Log.errorf("Failed to configure the http client for the domain %s\r\n",
                   TIMEZONE_URL);
        return;
    }

    Log.info("--- Configured to HTTP ---");

    HttpResponse response;
    response = HttpClient.get(TIMEZONE_URI);
    if (response.status_code != HttpClient.STATUS_OK) {
        Log.errorf("Error when performing a GET request on %s/%s. Got status "
                   "code = %d. Exiting...\r\n",
                   TIMEZONE_URL,
                   TIMEZONE_URI,
                   response.status_code);
        return;
    }

    Log.infof(
        "Successfully performed GET request. Status Code = %d, Size = %d\r\n",
        response.status_code,
        response.data_size);

    String body = HttpClient.readBody(512);

    if (body == "") {
        Log.errorf("The returned body from the GET request is empty. Something "
                   "went wrong. Exiting...\r\n");
        return;
    }

    Log.infof("Got the time (unixtime) %lu\r\n", getTimeFromApiresp(&body));
}

void loop() {}

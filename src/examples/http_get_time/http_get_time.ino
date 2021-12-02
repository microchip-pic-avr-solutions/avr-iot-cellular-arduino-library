#include <Arduino.h>
#include <http_client.h>
#include <lte.h>
#include <sequans_controller.h>
#include "log/log.h"

#define TIMEZONE_URL "worldtimeapi.org"
#define TIMEZONE_URI "/api/timezone/Europe/Oslo.txt"

long getTimeFromApiresp(String *resp)
{
	int unixTimeIndex = resp->indexOf(String("unixtime: "));
	int utx_datetimeIndex = resp->indexOf(String("utc_datetime"));

	String substr = resp->substring(unixTimeIndex + 10, utx_datetimeIndex - 1);
	Log5.Infof("Parsed Time = %d\n");
	return substr.toInt();
}

void setup()
{

	Serial5.begin(115200);
	Log5.setLogLevel(LogLevels::INFO);
	Log5.Info("Starting HTTP Get Time Example");

	// Start LTE modem and wait until we are connected to the operator
	Lte.begin();

	while (!Lte.isConnected())
	{
		Log5.Info("Not connected to operator yet...");
		delay(5000);
	}

	Log5.Info("Connected to operator!");

	if (!HttpClient.configure(TIMEZONE_URL, 80, false))
	{
		Log5.Errorf("Failed to configure the http client for the domain %s", TIMEZONE_URL);
		return;
	}

	Log5.Info("Configured to HTTP");

	HttpResponse response;
	response = HttpClient.get(TIMEZONE_URI);
	if (response.status_code != HttpClient.STATUS_OK)
	{
		Log5.Errorf("Error when performing a GET request on %s/%s. Got status code = %d. Exiting...", TIMEZONE_URL, TIMEZONE_URI, response.status_code);
		return;
	}

	Log5.Infof("Successfully performed GET request. Status Code = %d, Size = %d\n", response.status_code, response.data_size);

	String body = HttpClient.readBody(512);

	if (body == "")
	{
		Log5.Errorf("The returned body from the GET request is empty. Something went wrong. Exiting...");
		return;
	}

	Log5.Infof("Got the time (unixtime) %lu\n", getTimeFromApiresp(&body));
}

void loop()
{
	while (1)
		;
}

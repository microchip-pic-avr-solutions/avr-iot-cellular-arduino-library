#include <Arduino.h>
#include <http_client.h>
#include <lte.h>
#include "log/log.h"

#define TIMEZONE_URL "worldtimeapi.org"
#define TIMEZONE_URI "/api/timezone/Europe/Oslo.txt"

long getTimeFromApiresp(String *resp)
{
	int unixTimeIndex = resp->indexOf(String("unixtime: "));
	int utx_datetimeIndex = resp->indexOf(String("utc_datetime"));

	String substr = resp->substring(unixTimeIndex + 10, utx_datetimeIndex - 1);
	return substr.toInt();
}

void setup()
{

	LOG.begin(115200);
	LOG.setLogLevel(LogLevels::INFO);
	LOG.Info("Starting HTTP Get Time Example");

	// Start LTE modem and wait until we are connected to the operator
	Lte.begin();

	while (!Lte.isConnected())
	{
		LOG.Info("Not connected to operator yet...");
		delay(5000);
	}

	LOG.Info("Connected to operator!");

	if (!HttpClient.configure(TIMEZONE_URL, 80, false))
	{
		LOG.Errorf("Failed to configure the http client for the domain %s", TIMEZONE_URL);
		return;
	}

	LOG.Info("Configured to HTTP");

	HttpResponse response;
	response = HttpClient.get(TIMEZONE_URI);
	if (response.status_code != HttpClient.STATUS_OK)
	{
		LOG.Errorf("Error when performing a GET request on %s/%s. Got status code = %d. Exiting...", TIMEZONE_URL, TIMEZONE_URI, response.status_code);
		return;
	}

	LOG.Infof("Successfully performed GET request. Status Code = %d, Size = %d\n", response.status_code, response.data_size);

	String body = HttpClient.readBody(512);

	if (body == "")
	{
		LOG.Errorf("The returned body from the GET request is empty. Something went wrong. Exiting...");
		return;
	}

	LOG.Infof("Got the time (unixtime) %lu\n", getTimeFromApiresp(&body));
}

void loop()
{
	while (1)
		;
}

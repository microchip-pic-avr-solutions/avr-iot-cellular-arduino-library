/**
 * This demo utilizes the Adafruit Ultimate GPS Featherwing to send the GPS
 * position of the device to a server with HTTP. More information about this
 * example sketch at the documentation:
 * https://iot.microchip.com/docs/arduino/startup under Examples -> GPS Tracker.
 *
 * Remember to change HTTP_DOMAIN to your server IP!
 */

#include <Adafruit_GPS.h>
#include <Arduino.h>
#include <http_client.h>
#include <led_ctrl.h>
#include <log.h>
#include <low_power.h>
#include <lte.h>
#include <mcp9808.h>
#include <veml3328.h>

#define GPSSerial Serial2

#define HTTP_DOMAIN "<your server IP here>"

/**
 * @brief Connected refers to the LTE network.
 */
enum class State { NOT_CONNECTED, CONNECTED, CONNECTED_WITH_FIX };

/**
 * @brief Interface for the GPS.
 */
Adafruit_GPS GPS(&GPSSerial);

/**
 * @brief Due to no float string support, these variables are used with dtostrf
 * such that the float values for latitude and longitude are properly
 * converted before being sent to the server.
 */
static char latitude[16]  = "0";
static char longitude[16] = "0";
static char time[24]      = "0";

/**
 * @brief Keeps track of the state.
 */
static State state = State::NOT_CONNECTED;

/**
 * @brief Whether or not we've parsed one GPS entry. Prevents sending zeros
 * whilst having fix after boot.
 */
static bool has_parsed = false;

/**
 * @brief Starts the GPS modem and sets up the configuration.
 */
static void initializeGPS(void) {

    GPSSerial.swap(1);

    GPS.begin(9600);

    // Enable RMC & GGA output messages
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);

    // Set the update rate to 1Hz
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

    // Disable updates on antenna status
    GPS.sendCommand(PGCMD_NOANTENNA);

    delay(1000);

    // Ask for firmware version
    GPSSerial.println(PMTK_Q_RELEASE);
}

/**
 * @brief Sets up the HTTP client with the given domain
 */
static void initializeHTTP(void) {
    if (!HttpClient.configure(HTTP_DOMAIN, 8080, false)) {
        Log.info(F("Failed to configure HTTP client"));
    } else {
        Log.info(F("Configured HTTP"));
    }
}

/**
 * @brief Sends a payload with latitude, longitude and the timestamp.
 */
static void sendData(void) {

    char data[80] = "";

    sprintf_P(data,
              PSTR("{\"lat\":\"%s\",\"lon\":\"%s\",\"time\": \"%s\"}"),
              latitude,
              longitude,
              time);

    HttpResponse response = HttpClient.post("/data", data);

    Log.infof(F("POST - status code: %u, data size: %u\r\n"),
              response.status_code,
              response.data_size);

    if (response.status_code != 0) {
        String body = HttpClient.readBody(512);

        if (body != "") {
            Log.infof(F("Response: %s\r\n"), body.c_str());
        }
    }
}

/**
 * @brief Connects to the network operator. Will block until connection is
 * achieved.
 */
static void connectToNetwork() {

    // If we already are connected, don't do anything
    if (!Lte.isConnected()) {
        while (!Lte.begin()) {}
        Log.infof(F("Connected to operator: %s\r\n"),
                  Lte.getOperator().c_str());
    }

    state = State::CONNECTED;
}

/**
 * @brief Checks for new GPS messages and parses the NMEA messages if any.
 */
static void parseGPSMessages() {

    // Read the incoming messages, needn't do anything with them yet as that is
    // taken care of by the newNMEAReceived() function.
    if (GPS.available()) {
        GPS.read();
    }

    if (GPS.newNMEAreceived()) {
        if (!GPS.parse(GPS.lastNMEA())) {
            // If we fail to parse the NMEA, wait for the next one
            return;
        } else {

            Log.rawf(F("Timestamp: %d/%d/20%d %d:%d:%d GMT+0 \r\n"),
                     GPS.day,
                     GPS.month,
                     GPS.year,
                     GPS.hour,
                     GPS.minute,
                     GPS.seconds);

            Log.rawf(F("Fix: %d, quality: %d\r\n"), GPS.fix, GPS.fixquality);

            if (GPS.fix) {

                // Need to convert all floats to strings
                dtostrf(GPS.latitudeDegrees, 2, 4, latitude);
                dtostrf(GPS.longitudeDegrees, 2, 4, longitude);

                sprintf_P(time,
                          PSTR("%d/%d/20%d %d:%d:%d"),
                          GPS.day,
                          GPS.month,
                          GPS.year,
                          GPS.hour,
                          GPS.minute,
                          GPS.seconds);

                Log.rawf(F("Location: %s N, %s E\r\n"), latitude, longitude);
                Log.rawf(F("Satellites: %d\r\n"), GPS.satellites);

                Log.rawf(F("\r\n"));

                has_parsed = true;
            } else {
                Log.info(F("Waiting for GPS fix..."));
            }
        }
    }
}

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.begin(115200);
    Log.info(F("Starting AVR-IoT Cellular Adafruit GPS example"));

    // We configure the low power module for power down configuration, where
    // the LTE modem and the CPU will be powered down
    LowPower.configurePowerDown();

    // Make sure sensors are turned off
    Veml3328.begin();
    Mcp9808.begin();
    Veml3328.shutdown();
    Mcp9808.shutdown();

    initializeGPS();
    initializeHTTP();
}

void loop() {

    parseGPSMessages();

    switch (state) {
    case State::NOT_CONNECTED:
        connectToNetwork();
        break;

    case State::CONNECTED:
        if (!Lte.isConnected()) {
            state = State::NOT_CONNECTED;
        } else if (GPS.fix) {
            state = State::CONNECTED_WITH_FIX;
            LedCtrl.on(Led::CON);

            // Decrease update rate once we have fix to save power
            GPS.sendCommand(PMTK_SET_NMEA_UPDATE_100_MILLIHERTZ);
        }

        break;

    case State::CONNECTED_WITH_FIX:
        if (!Lte.isConnected()) {
            state = State::NOT_CONNECTED;
        } else if (!GPS.fix) {

            // Lost fix, set update rate back to 1 Hz
            GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

            state = State::CONNECTED;
            LedCtrl.off(Led::CON);
        }

        if (Lte.isConnected() && GPS.fix && has_parsed) {
            sendData();

            // Reset state before we power down, which will turn of the modem
            state = State::NOT_CONNECTED;
            LedCtrl.off(Led::CON);

            Log.info(F("Entering low power"));
            GPS.sendCommand(PMTK_STANDBY);
            delay(1000); // Allow some time to print messages before we sleep

            LowPower.powerDown(60);

            Log.info(F("Woke up!"));
            GPS.sendCommand(PMTK_AWAKE);

            // Set that we need an update for the position
            has_parsed = false;
        }

        break;
    }
}

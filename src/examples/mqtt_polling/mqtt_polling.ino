/**
 * This example uses polling for the LTE module and the MQTT module when
 * checking for new messages.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>

#define MQTT_USE_AWS   false
#define MQTT_SUB_TOPIC "mchp_topic_sub"
#define MQTT_PUB_TOPIC "mchp_topic_pub"

// If you are not using AWS, apply these settings
#if (!MQTT_USE_AWS)

#define MQTT_THING_NAME "someuniquemchp"
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_PORT       1883
#define MQTT_USE_TLS    false
#define MQTT_USE_ECC    false

#endif

bool connectedToBroker = false;

void setup() {
    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);
    Log.info("Starting initialization of MQTT Polling\r\n");

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Not connected to operator yet...\r\n");
        delay(5000);
    }

    Log.info("Connected to operator!\r\n");

// Attempt to connect to broker
#if (MQTT_USE_AWS)
    connectedToBroker = MqttClient.beginAWS();
#else
    connectedToBroker = MqttClient.begin(
        MQTT_THING_NAME, MQTT_BROKER, MQTT_PORT, MQTT_USE_TLS, MQTT_USE_ECC);
#endif

    if (connectedToBroker) {
        Log.info("Connecting to broker...\r\n");
        // TODO: Fails on connect...
        while (!MqttClient.isConnected()) {
            Log.info("Connecting...\r\n");
            delay(500);
        }
        Log.info("Connected to broker!\r\n");
        MqttClient.subscribe(MQTT_SUB_TOPIC);
    } else {
        Log.error("Failed to connect to broker\r\n");
    }
}

void loop() {

    if (connectedToBroker) {

        String message = MqttClient.readMessage(MQTT_SUB_TOPIC);

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a new
        // message

        if (message != "") {
            Log.info("Got new message: ");
            Log.info(message);
            Log.info("\r\n");
        }

// Publishing can fail due to network issues, so to be on the safe side
// one should check the return value to see if the message got published
#if ((MQTT_USE_ECC) || (MQTT_USE_AWS))
        // If we are using the ECC (secure element), we need to poll for
        // situations where the Sequans modem wants something signed.
        MqttClient.signIncomingRequests();
#endif

        bool publishedSuccessfully =
            MqttClient.publish(MQTT_PUB_TOPIC, "hello world");

        if (!publishedSuccessfully) {
            Log.error("Failed to publish\r\n");
        }
    }

    delay(2000);
}

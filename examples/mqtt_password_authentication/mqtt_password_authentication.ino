/**
 * @brief This example uses username and password authentication to connect to a
 * MQTT server.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>

#define MQTT_SUB_TOPIC "mchp_topic"
#define MQTT_PUB_TOPIC "mchp_topic"

#define MQTT_THING_NAME    "someuniquemchp"
#define MQTT_BROKER        "test.mosquitto.org"
#define MQTT_PORT          1884
#define MQTT_USE_TLS       false
#define MQTT_KEEPALIVE     60
#define MQTT_USE_ECC       false
#define MOSQUITTO_USERNAME "rw"
#define MOSQUITTO_PASSWORD "readwrite"

static uint32_t counter = 0;

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting initialization of MQTT with username and password");

    // Establish LTE connection
    if (!Lte.begin()) {
        Log.error("Failed to connect to operator");

        // Halt here
        while (1) {}
    }

    // Attempt to connect to the broker
    if (MqttClient.begin(MQTT_THING_NAME,
                         MQTT_BROKER,
                         MQTT_PORT,
                         MQTT_USE_TLS,
                         MQTT_KEEPALIVE,
                         MQTT_USE_ECC,
                         MOSQUITTO_USERNAME,
                         MOSQUITTO_PASSWORD)) {

        Log.infof("Connecting to broker");
        while (!MqttClient.isConnected()) {
            Log.rawf(".");
            LedCtrl.toggle(Led::CON);
            delay(500);
        }

        Log.rawf(" OK!\r\n");

        if (MqttClient.subscribe(MQTT_SUB_TOPIC)) {
            Log.infof("Subscribed to %s\r\n", MQTT_SUB_TOPIC);
        } else {
            Log.error("Failed to subscribe to topic");

            // Halt here
            while (1) {}
        }
    } else {
        Log.rawf("\r\n");
        Log.error("Failed to connect to broker");

        // Halt here
        while (1) {}
    }
}

void loop() {

    String message_to_publish = String("Hello world: " + String(counter));

    bool publishedSuccessfully = MqttClient.publish(MQTT_PUB_TOPIC,
                                                    message_to_publish.c_str());

    if (publishedSuccessfully) {
        Log.infof("Published message: %s\r\n", message_to_publish.c_str());
        counter++;
    } else {
        Log.error("Failed to publish");
    }

    delay(2000);

    String message = MqttClient.readMessage(MQTT_SUB_TOPIC);

    // Read message will return an empty string if there were no new
    // messages, so anything other than that means that there was a new
    // message
    if (message != "") {
        Log.infof("Got new message: %s\r\n", message.c_str());
    }

    delay(2000);
}

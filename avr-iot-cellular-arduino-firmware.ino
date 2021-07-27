#include "src/lte/lte.h"
#include "src/lte/mqtt_client.h"
#include "test.h"

#include <Arduino.h>

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3
#define DATA_LED       PIN_PG4
#define ERROR_LED      PIN_PG5

MqttClient mqtt_client;
bool initialized_mqtt_client = false;
bool should_check_message = false;

void receive(void) {
    should_check_message = true;

    digitalWrite(DATA_LED, LOW);
}

void initializeMqttClient() {

    digitalWrite(ERROR_LED, HIGH);

    if (mqtt_client.begin("iotthing",
                          "a2o6d3azuiiax4-ats.iot.us-east-2.amazonaws.com",
                          8883,
                          true)) {
        Serial5.println("Connected to MQTT broker");

        mqtt_client.registerReceiveNotificationCallback(receive);
        mqtt_client.subscribe("sdk/test/Python");

        digitalWrite(CONNECTION_LED, LOW);
    } else {
        Serial5.println("Failed to configure and connect MQTT");
        digitalWrite(ERROR_LED, LOW);
    }
}

void publishMessage(const char *topic, const char *message) {

    digitalWrite(ERROR_LED, HIGH);
    digitalWrite(DATA_LED, LOW);

    if (mqtt_client.publish(topic, (uint8_t *)message, strlen(message))) {
        Serial5.println("Published to MQTT broker");
    } else {
        Serial5.println("Failed to publish");
        digitalWrite(ERROR_LED, LOW);
    }

    digitalWrite(DATA_LED, HIGH);
}

bool should_check_connection = true;

void updateConnectionStatus(void) { should_check_connection = true; }

void setup() {
    Serial5.begin(115200);

    // These pins is active low
    pinMode(CELL_LED, OUTPUT);
    pinMode(CONNECTION_LED, OUTPUT);
    pinMode(DATA_LED, OUTPUT);
    pinMode(ERROR_LED, OUTPUT);

    digitalWrite(CELL_LED, HIGH);
    digitalWrite(CONNECTION_LED, HIGH);
    digitalWrite(DATA_LED, HIGH);
    digitalWrite(ERROR_LED, HIGH);

    LTE.begin();
    LTE.registerConnectionNotificationCallback(updateConnectionStatus);

    Serial5.println("---- Finished initializing ----");
}

void loop() {

    debugBridgeUpdate();

    if (should_check_connection) {
        // Pin is active low
        bool connected = LTE.isConnectedToOperator();
        digitalWrite(CELL_LED, connected ? LOW : HIGH);
        should_check_connection = false;

        if (connected && !initialized_mqtt_client) {
            initializeMqttClient();

            initialized_mqtt_client = true;
        }
    }

    if (should_check_message) {

        MqttReceiveNotification notification =
            mqtt_client.readReceiveNotification();

        // Failed to read notification or some error happened
        if (notification.message_length == 0) {
            return;
        }

        // Extra space for termination
        char buffer[notification.message_length + 16] = "";

        if (mqtt_client.readMessage(
                notification.receive_topic.c_str(), buffer, sizeof(buffer))) {
            Serial5.printf("I got the messsage: %s\r\n", (char *)buffer);

            // We publish a message back
            publishMessage("heartbeat", buffer);
        } else {
            Serial5.printf("Failed to read message\r\n");
        }

        should_check_message = false;

        digitalWrite(DATA_LED, HIGH);
    }
}

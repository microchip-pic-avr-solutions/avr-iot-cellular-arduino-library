/**
 * This example is a more advanced MQTT example with callbacks/interrupts and a
 * state machine. It will just listen to a topic and send the messages on that
 * topic back on another topic.
 */

#include "log/log.h"
#include <Arduino.h>
#include <lte.h>
#include <mqtt_client.h>
#include <sequans_controller.h>

#define MQTT_USE_AWS   false
#define MQTT_SUB_TOPIC "mchp_topic_sub"
#define MQTT_PUB_TOPIC "mchp_topic_pub"

// If you are not using AWS, apply these settings
#if (!MQTT_USE_AWS)

#define MQTT_THING_NAME "myMchpThing"
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_PORT       1883
#define MQTT_USE_TLS    false
#define MQTT_USE_ECC    false

#endif

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3

#define NETWORK_CONN_FLAG    1 << 0
#define NETWORK_DISCONN_FLAG 1 << 1
#define BROKER_CONN_FLAG     1 << 2
#define BROKER_DISCONN_FLAG  1 << 3
#define RECEIVE_MSG_FLAG     1 << 4

typedef enum { NOT_CONNECTED, CONNECTED_TO_NETWORK, CONNECTED_TO_BROKER } State;

State state = NOT_CONNECTED;
uint8_t callback_flags = 0;

// -------------------------- CALLBACKS & SETUP ---------------------------- //

void connectedToNetwork(void) { callback_flags |= NETWORK_CONN_FLAG; }
void disconnectedFromNetwork(void) { callback_flags |= NETWORK_DISCONN_FLAG; }

void connectedToBroker(void) { callback_flags |= BROKER_CONN_FLAG; }
void disconnectedFromBroker(void) { callback_flags |= BROKER_DISCONN_FLAG; }

void receive(void) { callback_flags |= RECEIVE_MSG_FLAG; }

void setup() {
    Serial5.begin(115200);
    Log5.setLogLevel(LogLevels::INFO);
    Log5.Info("Starting initialization of MQTT Interrupt");

    pinMode(CELL_LED, OUTPUT);
    pinMode(CONNECTION_LED, OUTPUT);

    // These pins is active low
    digitalWrite(CELL_LED, HIGH);
    digitalWrite(CONNECTION_LED, HIGH);

    // Register callbacks for network connection
    Lte.onConnectionStatusChange(connectedToNetwork, disconnectedFromNetwork);
    Lte.begin();
}

// ----------------------------- STATE MACHINE ------------------------------ //

void loop() {

    if (callback_flags & NETWORK_CONN_FLAG) {
        switch (state) {
        case NOT_CONNECTED:
            state = CONNECTED_TO_NETWORK;
            digitalWrite(CELL_LED, LOW);

            MqttClient.onConnectionStatusChange(connectedToBroker,
                                                disconnectedFromBroker);
            MqttClient.onReceive(receive);

// Attempt connection to MQTT broker
// Attempt to connect to broker
#if (MQTT_USE_AWS)
            if (MqttClient.beginAWS())
#else
            if (MqttClient.begin(MQTT_THING_NAME,
                                 MQTT_BROKER,
                                 MQTT_PORT,
                                 MQTT_USE_TLS,
                                 MQTT_USE_ECC))
#endif
            {
                Log5.Info("Connecting to broker...");
                while (!MqttClient.isConnected()) {
                    Log5.Info("Connecting...");
                    delay(500);
                }
                MqttClient.subscribe(MQTT_SUB_TOPIC);
            } else {
                Log5.Error("Failed to connect to broker");
            }

            break;
        }

        callback_flags &= ~NETWORK_CONN_FLAG;
    } else if (callback_flags & NETWORK_DISCONN_FLAG) {
        switch (state) {
        default:
            MqttClient.end();
            state = NOT_CONNECTED;
            digitalWrite(CONNECTION_LED, HIGH);
            digitalWrite(CELL_LED, HIGH);
            break;
        }

        callback_flags &= ~NETWORK_DISCONN_FLAG;
    } else if (callback_flags & BROKER_CONN_FLAG) {
        switch (state) {

        case CONNECTED_TO_NETWORK:
            state = CONNECTED_TO_BROKER;
            digitalWrite(CONNECTION_LED, LOW);
            break;
        }

        callback_flags &= ~BROKER_CONN_FLAG;
    } else if (callback_flags & BROKER_DISCONN_FLAG) {

        switch (state) {

        case CONNECTED_TO_BROKER:
            state = CONNECTED_TO_NETWORK;
            digitalWrite(CONNECTION_LED, HIGH);
            break;
        }

        callback_flags &= ~BROKER_DISCONN_FLAG;
    } else if (callback_flags & RECEIVE_MSG_FLAG) {

        switch (state) {
        case CONNECTED_TO_BROKER:

            MqttReceiveNotification notification =
                MqttClient.readReceiveNotification();

            // Failed to read notification or some error happened
            if (notification.message_length == 0) {
                return;
            }

            // Extra space for termination
            char buffer[notification.message_length + 16] = "";

            if (MqttClient.readMessage(notification.receive_topic.c_str(),
                                       buffer,
                                       sizeof(buffer))) {
                Log5.Infof("I got the messsage: %s\r\n", (char *)buffer);

                // We publish the message back
                MqttClient.publish(MQTT_PUB_TOPIC, buffer);
            } else {
                Log5.Error("Failed to read message\r\n");
            }

            break;
        }

        callback_flags &= ~RECEIVE_MSG_FLAG;
    }

#if ((MQTT_USE_ECC == TRUE) || (MQTT_USE_AWS))
    // If we are using the ECC (secure element), we need to poll for situations
    // where the Sequans modem wants something signed.
    MqttClient.pollSign();
#endif
}

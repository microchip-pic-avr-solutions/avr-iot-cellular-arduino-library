/**
 * This example is a more advanced MQTT example with callbacks/interrupts and a
 * state machine. It will just listen to a topic and send the messages on that
 * topic back on another topic.
 */

#include <Arduino.h>
#include <log.h>
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

#ifdef __AVR_AVR128DB48__ // MINI

#define CELL_LED       PIN_PA0
#define CONNECTION_LED PIN_PA1

#else
#ifdef __AVR_AVR128DB64__ // NON-MINI

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

#define NETWORK_CONN_FLAG    1 << 0
#define NETWORK_DISCONN_FLAG 1 << 1
#define BROKER_CONN_FLAG     1 << 2
#define BROKER_DISCONN_FLAG  1 << 3
#define RECEIVE_MSG_FLAG     1 << 4

typedef enum { NOT_CONNECTED, CONNECTED_TO_NETWORK, CONNECTED_TO_BROKER } State;

State state = NOT_CONNECTED;
uint8_t callback_flags = 0;

static char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 1] = "";
static uint16_t message_length = 0;

// -------------------------- CALLBACKS & SETUP ---------------------------- //

void connectedToNetwork(void) { callback_flags |= NETWORK_CONN_FLAG; }
void disconnectedFromNetwork(void) { callback_flags |= NETWORK_DISCONN_FLAG; }

void connectedToBroker(void) { callback_flags |= BROKER_CONN_FLAG; }
void disconnectedFromBroker(void) { callback_flags |= BROKER_DISCONN_FLAG; }

void receive(const char *topic,
             const uint16_t msg_length,
             const int32_t message_id) {
    // Message ID is not used here, as we don't specify a MQTT Quality of
    // Service different from the default one. Read more about this in the
    // documentation for the onReceive() function in MqttClient
    memcpy(topic_buffer, topic, MQTT_TOPIC_MAX_LENGTH);
    message_length = msg_length;

    callback_flags |= RECEIVE_MSG_FLAG;
}

void setup() {
    Log.begin(115200);
    Log.info("Starting initialization of MQTT Interrupt\r\n");

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

                Log.info("Connecting to broker...\r\n");
                while (!MqttClient.isConnected()) {
                    Log.info("Connecting...\r\n");
                    delay(500);
                }
                MqttClient.subscribe(MQTT_SUB_TOPIC);
            } else {
                Log.error("Failed to connect to broker\r\n");
            }

            break;
        default:
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
        default:
            break;
        }

        callback_flags &= ~BROKER_CONN_FLAG;
    } else if (callback_flags & BROKER_DISCONN_FLAG) {

        switch (state) {

        case CONNECTED_TO_BROKER:
            state = CONNECTED_TO_NETWORK;
            digitalWrite(CONNECTION_LED, HIGH);
            break;
        default:
            break;
        }

        callback_flags &= ~BROKER_DISCONN_FLAG;
    } else if (callback_flags & RECEIVE_MSG_FLAG) {

        switch (state) {
        case CONNECTED_TO_BROKER: {

            // Extra space for termination
            char buffer[message_length + 16] = "";

            if (MqttClient.readMessage(
                    topic_buffer, (uint8_t *)buffer, sizeof(buffer))) {
                Log.infof("I got the messsage: %s\r\n", (char *)buffer);

                // We publish the message back
                MqttClient.publish(MQTT_PUB_TOPIC, buffer);
            } else {
                Log.error("Failed to read message\r\n");
            }

        } break;

        default:
            break;
        }

        callback_flags &= ~RECEIVE_MSG_FLAG;
    }

#if ((MQTT_USE_ECC == TRUE) || (MQTT_USE_AWS))
    // If we are using the ECC (secure element), we need to poll for situations
    // where the Sequans modem wants something signed.
    MqttClient.signIncomingRequests();
#endif
}

#include <lte.h>
#include <mqtt_client.h>
#include <sequans_controller.h>

#include <Arduino.h>

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3
#define DATA_LED       PIN_PG4
#define ERROR_LED      PIN_PG5

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    128
#define RESPONSE_BUFFER_SIZE 256

bool should_check_message = false;

void connectedToNetwork(void) { digitalWrite(CELL_LED, LOW); }

void disconnectedFromNetwork(void) { digitalWrite(CELL_LED, HIGH); }

void connectedToBroker(void) { digitalWrite(CONNECTION_LED, LOW); }

void disconnectedFromBroker(void) { digitalWrite(CONNECTION_LED, HIGH); }

void receive(void) {
    should_check_message = true;
    digitalWrite(DATA_LED, LOW);
}

void publishMessage(const char *topic, const char *message) {

    digitalWrite(ERROR_LED, HIGH);
    digitalWrite(DATA_LED, LOW);

    if (MqttClient.publish(topic, (uint8_t *)message, strlen(message))) {
        Serial5.println("Published to MQTT broker");
    } else {
        Serial5.println("Failed to publish");
        digitalWrite(ERROR_LED, LOW);
    }

    digitalWrite(DATA_LED, HIGH);
}

void setupPins(void) {
    // These pins is active low
    pinMode(CELL_LED, OUTPUT);
    pinMode(CONNECTION_LED, OUTPUT);
    pinMode(DATA_LED, OUTPUT);
    pinMode(ERROR_LED, OUTPUT);

    digitalWrite(CELL_LED, HIGH);
    digitalWrite(CONNECTION_LED, HIGH);
    digitalWrite(DATA_LED, HIGH);
    digitalWrite(ERROR_LED, HIGH);
}

void setup() {
    Serial5.begin(115200);
    setupPins();

    Lte.onConnectionStatusChange(connectedToNetwork, disconnectedFromNetwork);
    Lte.begin();

    MqttClient.onConnectionStatusChange(connectedToBroker,
                                        disconnectedFromBroker);
    MqttClient.onReceive(receive);

    // TODO: this should be called after we are connected
    if (MqttClient.begin("avrdb64",
                         "a2o6d3azuiiax4-ats.iot.us-east-2.amazonaws.com",
                         8883,
                         true)) {

        // TODO: This should be issued after connection
        // MqttClient.subscribe("sdk/test/Python");
    } else {
        digitalWrite(ERROR_LED, LOW);
        Serial5.println("Failed to configure MQTT");
    }

    Serial5.println("---- Finished initializing ----");

    if (Lte.isConnected()) {
        digitalWrite(CELL_LED, LOW);
    }
}

void debugBridgeUpdate(void) {
    static uint8_t character;
    static char input_buffer[INPUT_BUFFER_SIZE];
    static uint8_t input_buffer_index = 0;

    if (Serial5.available() > 0) {
        character = Serial5.read();

        switch (character) {
        case DEL_CHARACTER:
            if (strlen(input_buffer) > 0) {
                input_buffer[input_buffer_index--] = 0;
            }
            break;

        case ENTER_CHARACTER:

            /*
            if (memcmp(input_buffer, "http", 4) == 0) {
                testHttp();
            } else if (memcmp(input_buffer, "twi", 3) == 0) {
                testTwi();
            } else {
            */
            SequansController.writeCommand(input_buffer);
            //}

            // Reset buffer
            memset(input_buffer, 0, sizeof(input_buffer));
            input_buffer_index = 0;

            break;

        default:
            input_buffer[input_buffer_index++] = character;
            break;
        }

        Serial5.print((char)character);
    }

    if (SequansController.isRxReady()) {
        // Send back data from modem to host
        Serial5.write(SequansController.readByte());
    }
}

void loop() {

    debugBridgeUpdate();

    if (should_check_message) {

        MqttReceiveNotification notification =
            MqttClient.readReceiveNotification();

        // Failed to read notification or some error happened
        if (notification.message_length == 0) {
            return;
        }

        // Extra space for termination
        char buffer[notification.message_length + 16] = "";

        if (MqttClient.readMessage(
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

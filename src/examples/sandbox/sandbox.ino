/**
 * This example is the code that runs during the initial landing page / sandbox
 * experience
 */

#define SANDBOX_VERSION "1.0.0"

#include <Arduino.h>

#include <ArduinoJson.h>
#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mcp9808.h>
#include <mqtt_client.h>

#define HEARTBEAT_INTERVAL_MS 10000

// AWS defines which topic you are allowed to subscribe and publish too. This is
// defined by the policy The default policy with the Microchip IoT Provisioning
// Tool allows for publishing and subscribing on thing_id/topic. If you want to
// publish and subscribe on other topics, see the AWS IoT Core Policy
// documentation.
#define MQTT_SUB_TOPIC_FMT "$aws/things/%s/shadow/update/delta"
#define MQTT_PUB_TOPIC_FMT "%s/sensors"

#define MQTT_MESSAGE_BUFFERS      4
#define MQTT_MESSAGE_BUFFERS_MASK (MQTT_MESSAGE_BUFFERS - 1)

#define NETWORK_CONN_FLAG                 1 << 0
#define NETWORK_DISCONN_FLAG              1 << 1
#define BROKER_CONN_FLAG                  1 << 2
#define BROKER_DISCONN_FLAG               1 << 3
#define RECEIVE_MSG_FLAG                  1 << 4
#define SEND_HEARTBEAT_FLAG               1 << 5
#define STOP_PUBLISHING_SENSOR_DATA_FLAG  1 << 6
#define START_PUBLISHING_SENSOR_DATA_FLAG 1 << 7
#define SEND_SENSOR_DATA_FLAG             1 << 8

typedef enum {
    NOT_CONNECTED,
    CONNECTED_TO_NETWORK,
    CONNECTED_TO_BROKER,
    STREAMING_DATA
} State;

static State state = NOT_CONNECTED;

static volatile uint16_t event_flags = 0;

static char mqtt_sub_topic[128];
static char mqtt_pub_topic[128];

static unsigned long last_heartbeat_time = 0;

static volatile uint16_t seconds_counted = 0;
static volatile uint16_t target_seconds = 0;
static volatile uint16_t data_frequency = 0;
static unsigned long last_data_time = 0;

static char topic_buffer[MQTT_MESSAGE_BUFFERS][MQTT_TOPIC_MAX_LENGTH + 1] =
    {"", "", "", ""};
static volatile uint16_t message_length[MQTT_MESSAGE_BUFFERS] = {0, 0, 0, 0};
static volatile int32_t message_id[MQTT_MESSAGE_BUFFERS] = {-1, -1, -1, -1};
static volatile uint8_t message_head_index = 0;
static volatile uint8_t message_tail_index = 0;

static const char heartbeat_message[] = "{\"type\": \"heartbeat\"}";

char transmit_buffer[256];

ISR(TCA0_OVF_vect) {
    seconds_counted++;

    if (seconds_counted == target_seconds) {
        event_flags |= STOP_PUBLISHING_SENSOR_DATA_FLAG;
    }

    TCA0.SINGLE.INTFLAGS |= (1 << 0);
}

ISR(PORTD_PORT_vect) {
    if (PORTD.INTFLAGS & (1 << 2)) {

        event_flags |= SEND_HEARTBEAT_FLAG;
        PORTD.INTFLAGS |= (1 << 2);
    }
}

void connectedToNetwork(void) { event_flags |= NETWORK_CONN_FLAG; }
void disconnectedFromNetwork(void) { event_flags |= NETWORK_DISCONN_FLAG; }
void connectedToBroker(void) { event_flags |= BROKER_CONN_FLAG; }
void disconnectedFromBroker(void) { event_flags |= BROKER_DISCONN_FLAG; }

void receivedMessage(const char *topic,
                     const uint16_t msg_length,
                     const int32_t msg_id) {

    memcpy(topic_buffer[message_head_index], topic, MQTT_TOPIC_MAX_LENGTH);
    message_length[message_head_index] = msg_length;
    message_id[message_head_index] = msg_id;

    message_head_index = (message_head_index + 1) & MQTT_MESSAGE_BUFFERS_MASK;
}

void connectMqtt() {
    MqttClient.onConnectionStatusChange(connectedToBroker,
                                        disconnectedFromBroker);
    MqttClient.onReceive(receivedMessage);

    // Attempt to connect to broker
    MqttClient.beginAWS();
}

void connectLTE() {

    Lte.onConnectionStatusChange(connectedToNetwork, disconnectedFromNetwork);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
}

void startStreamTimer() {
    // We need to tell the core that we're "taking over" the TCA0 timer
    takeOverTCA0();

    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;

    // Count per second (24MHz)
    TCA0.SINGLE.PER = 23437;

    sei();
    // Enable the timer with a division of 1024
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
}

void stopStreamTimer() { TCA0.SINGLE.CTRLA = 0; }

void decodeMessage(const char *message) {
    StaticJsonDocument<800> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Log.errorf("Unable to deserialize received JSON: %s\r\n",
                   error.f_str());
        return;
    }

    // Handle command frame
    const char *cmd = doc["state"]["cmd"];
    if (cmd == 0) {
        Log.errorf("Unable to get command, pointer is zero, "
                   "message is %s\r\n",
                   message);
        return;
    }

    // If it's a toggle_led command, handle it
    if (strcmp(cmd, "set_led") == 0) {
        // -- Read the led value
        const char *target_led = doc["state"]["opts"]["led"];
        const unsigned int target_state =
            doc["state"]["opts"]["state"].as<unsigned int>();

        if (target_led == 0) {
            Log.errorf("Unable to get target led or state, pointer is zero, "
                       "message is %s\r\n",
                       message);
            return;
        }

        // -- Toggle LED based on the given value
        Led led;

        if (strcmp(target_led, "USER") == 0) {
            led = Led::USER;
        } else if (strcmp(target_led, "ERROR") == 0) {
            led = Led::ERROR;
        } else {
            Log.errorf("Invalid LED value provided, "
                       "led provided = %s\r\n",
                       led);
            return;
        }

        if (target_state) {
            Log.infof("Turning LED %s on\r\n", target_led);
            LedCtrl.on(led);
        } else {
            Log.infof("Turning LED %s off\r\n", target_led);
            LedCtrl.off(led);
        }

    } else if (strcmp(cmd, "stream") == 0) {
        const unsigned int duration =
            doc["state"]["opts"]["duration"].as<unsigned int>();

        const unsigned int frequency =
            doc["state"]["opts"]["freq"].as<unsigned int>();

        if (duration == 0 || frequency == 0) {
            Log.errorf("Unable to get duration or frequency, pointer "
                       "is zero, message is %s\r\n",
                       message);
            return;
        }

        data_frequency = frequency;
        seconds_counted = 0;
        target_seconds = duration;

        event_flags |= START_PUBLISHING_SENSOR_DATA_FLAG;
    } else if (strcmp(cmd, "verbose_logs") == 0) {
        Log.setLogLevel(LogLevel::DEBUG);
    }
}

void setup() {
    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Set PD2 as input (button)
    pinConfigure(PIN_PD2, PIN_DIR_INPUT | PIN_INT_FALL);

    Log.infof("Starting sandbox / landing page procedure. Version = %s\r\n",
              SANDBOX_VERSION);

    if (Mcp9808.begin(0x18) == -1) {
        Log.error("Could not initialize the temperature sensor");
    }

    ECC608.begin();

    // Find the thing ID and set the publish and subscription topics
    uint8_t thing_name[128];
    uint8_t thing_name_len = sizeof(thing_name);

    uint8_t err = ECC608.getThingName(thing_name, &thing_name_len);
    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve thing name from the ECC");
        Log.error("Unable to initialize the MQTT topics. Stopping...");
        return;
    }

    Log.infof("Board name: %s\r\n", thing_name);

    sprintf(mqtt_sub_topic, MQTT_SUB_TOPIC_FMT, thing_name);
    sprintf(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, thing_name);

    Log.info("Connecting to LTE network...");
    connectLTE();
}

void loop() {

    if (event_flags & NETWORK_CONN_FLAG) {
        switch (state) {
        case NOT_CONNECTED:
            state = CONNECTED_TO_NETWORK;
            Log.infof("Connected to operator: %s\r\n",
                      Lte.getOperator().c_str());
            Log.info("Connecting to MQTT broker...");
            connectMqtt();
            break;
        default:
            break;
        }

        event_flags &= ~NETWORK_CONN_FLAG;
    } else if (event_flags & NETWORK_DISCONN_FLAG) {
        switch (state) {
        default:
            state = NOT_CONNECTED;
            LedCtrl.off(Led::CELL);
            LedCtrl.off(Led::CON);
            LedCtrl.off(Led::DATA);
            LedCtrl.off(Led::USER);
            LedCtrl.off(Led::ERROR);

            Log.info("Network disconnection, attempting to reconnect...");

            Lte.end();
            connectLTE();
            break;
        }

        event_flags &= ~NETWORK_DISCONN_FLAG;
    } else if (event_flags & BROKER_CONN_FLAG) {

        switch (state) {
        case CONNECTED_TO_NETWORK:
            state = CONNECTED_TO_BROKER;

            Log.info("Connected to MQTT broker, subscribing to topics!\r\n");

            MqttClient.subscribe(mqtt_sub_topic, MqttQoS::AT_LEAST_ONCE);

            break;
        default:
            break;
        }

        event_flags &= ~BROKER_CONN_FLAG;
    } else if (event_flags & BROKER_DISCONN_FLAG) {

        switch (state) {
        case CONNECTED_TO_BROKER:
            state = CONNECTED_TO_NETWORK;

            Log.info("Lost connection to broker, attempting to reconnect...");

            MqttClient.end();
            connectMqtt();

            break;

        case STREAMING_DATA:
            state = CONNECTED_TO_NETWORK;

            Log.info("Lost connection to broker, attempting to reconnect...");

            stopStreamTimer();
            MqttClient.end();
            connectMqtt();

            break;

        default:
            break;
        }

        event_flags &= ~BROKER_DISCONN_FLAG;
    } else if (message_head_index != message_tail_index) {

        switch (state) {
        case CONNECTED_TO_BROKER:
        case STREAMING_DATA: {

            // Extra space for termination
            char message[message_length[message_tail_index] + 16] = "";

            if (!MqttClient.readMessage(topic_buffer[message_tail_index],
                                        (uint8_t *)message,
                                        sizeof(message),
                                        message_id[message_tail_index])) {

                Log.error("Failed to read message\r\n");
            }

            decodeMessage(message);

            message_tail_index =
                (message_tail_index + 1) & MQTT_MESSAGE_BUFFERS_MASK;

        } break;

        default:
            break;
        }

        event_flags &= ~RECEIVE_MSG_FLAG;
    } else if (event_flags & SEND_HEARTBEAT_FLAG) {

        switch (state) {

        case CONNECTED_TO_BROKER:
        case STREAMING_DATA:

            Log.info("Sending hearbeat");
            MqttClient.publish(mqtt_pub_topic, heartbeat_message);
            last_heartbeat_time = millis();

            break;

        default:
            break;
        }

        event_flags &= ~SEND_HEARTBEAT_FLAG;
    } else if (event_flags & START_PUBLISHING_SENSOR_DATA_FLAG) {
        switch (state) {
        case CONNECTED_TO_BROKER:

            state = STREAMING_DATA;

            Log.infof("Starting to stream data for %d seconds\r\n",
                      target_seconds);
            startStreamTimer();
            break;

        default:
            break;
        }

        event_flags &= ~START_PUBLISHING_SENSOR_DATA_FLAG;
    } else if (event_flags & STOP_PUBLISHING_SENSOR_DATA_FLAG) {

        switch (state) {
        case STREAMING_DATA:
            state = CONNECTED_TO_BROKER;
            break;
        default:
            break;
        }

        stopStreamTimer();
        event_flags &= ~STOP_PUBLISHING_SENSOR_DATA_FLAG;

    } else if (event_flags & SEND_SENSOR_DATA_FLAG) {
        switch (state) {
        case STREAMING_DATA:

            last_data_time = millis();

            // -- Warning: Doing sprintf on a pointer without checking for
            // overflow is *bad* practice, but we do it here due to the
            // simplicity of the data.
            sprintf(transmit_buffer,
                    "{\"type\": \"data\",\
                        \"data\": { \
                            \"Temperature\": %d \
                        } \
                    }",
                    int(Mcp9808.readTempC()));

            if (!MqttClient.publish(mqtt_pub_topic, transmit_buffer)) {
                Log.errorf("Could not publish message: %s\r\n",
                           transmit_buffer);
            }

            break;
        default:
            break;
        }

        event_flags &= ~SEND_SENSOR_DATA_FLAG;
    }

    switch (state) {
    case CONNECTED_TO_BROKER:

        if ((millis() - last_heartbeat_time) > HEARTBEAT_INTERVAL_MS) {
            event_flags |= SEND_HEARTBEAT_FLAG;
        }
        break;

    case STREAMING_DATA:
        if ((millis() - last_heartbeat_time) > HEARTBEAT_INTERVAL_MS) {
            event_flags |= SEND_HEARTBEAT_FLAG;
        }

        if ((millis() - last_data_time) > data_frequency) {
            event_flags |= SEND_SENSOR_DATA_FLAG;
        }

        break;

    default:
        break;
    }
}

/**
 * This example is the code that runs during the initial landing page / sandbox
 * experience
 */

#define SANDBOX_VERSION "1.3.10"

#include <ArduinoJson.h>
#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mcp9808.h>
#include <mqtt_client.h>
#include <veml3328.h>

#define HEARTBEAT_INTERVAL_MS 10000

// AWS defines which topic you are allowed to subscribe and publish too. This is
// defined by the policy The default policy with the Microchip IoT Provisioning
// Tool allows for publishing and subscribing on thing_id/topic. If you want to
// publish and subscribe on other topics, see the AWS IoT Core Policy
// documentation.
const char MQTT_SUB_TOPIC_FMT[] PROGMEM = "$aws/things/%s/shadow/update/delta";
const char MQTT_PUB_TOPIC_FMT[] PROGMEM = "%s/sensors";

#define NETWORK_CONN_FLAG                 (1 << 0)
#define NETWORK_DISCONN_FLAG              (1 << 1)
#define BROKER_DISCONN_FLAG               (1 << 3)
#define SEND_HEARTBEAT_FLAG               (1 << 4)
#define STOP_PUBLISHING_SENSOR_DATA_FLAG  (1 << 5)
#define START_PUBLISHING_SENSOR_DATA_FLAG (1 << 6)
#define SEND_SENSOR_DATA_FLAG             (1 << 7)

// Allow 32 messsages in flight
#define RECEIVE_MESSAGE_ID_BUFFER_SIZE 32
#define RECEIVE_MESSAGE_ID_BUFFER_MASK (RECEIVE_MESSAGE_ID_BUFFER_SIZE - 1)

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
static volatile uint16_t target_seconds  = 0;
static volatile uint16_t data_frequency  = 0;
static unsigned long last_data_time      = 0;

/**
 * @brief Circular buffer for the message identifiers of the received messages.
 */
static uint32_t received_message_identifiers[RECEIVE_MESSAGE_ID_BUFFER_SIZE];
static volatile uint8_t received_message_identifiers_head = 0;
static volatile uint8_t received_message_identifiers_tail = 0;

static const char heartbeat_message[] = "{\"type\": \"heartbeat\"}";

char transmit_buffer[256];

ISR(TCA0_OVF_vect) {
    seconds_counted++;

    if (seconds_counted == target_seconds) {
        event_flags |= STOP_PUBLISHING_SENSOR_DATA_FLAG;
    }

    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

void sendHeartbeatInterrupt(void) {
    if (PORTD.INTFLAGS & PIN2_bm) {
        PORTD.INTFLAGS = PIN2_bm;
        event_flags |= SEND_HEARTBEAT_FLAG;
    }
}

void resetInterrupt(void) {
    if (PORTF.INTFLAGS & PIN6_bm) {
        PORTF.INTFLAGS = PIN6_bm;
        asm("jmp 0");
    }
}

void disconnectedFromNetwork(void) { event_flags |= NETWORK_DISCONN_FLAG; }
void disconnectedFromBroker(void) { event_flags |= BROKER_DISCONN_FLAG; }

void receivedMessage(__attribute__((unused)) const char* topic,
                     __attribute__((unused)) const uint16_t msg_length,
                     const int32_t msg_id) {

    received_message_identifiers_head = (received_message_identifiers_head +
                                         1) &
                                        RECEIVE_MESSAGE_ID_BUFFER_MASK;

    received_message_identifiers[received_message_identifiers_head] = msg_id;
}

void connectMqtt() {
    MqttClient.onDisconnect(disconnectedFromBroker);
    MqttClient.onReceive(receivedMessage);

    // Attempt to connect to broker
    // Do this in a loop so that we retry if it fails
    while (!MqttClient.beginAWS()) { delay(1000); }
}

void connectLTE() {

    Lte.onDisconnect(disconnectedFromNetwork);

    // Start LTE modem and wait until we are connected to the operator.
    // If initialization fails, we just retry in the loop
    while (!Lte.begin()) {}

    // Signal that we are connected
    event_flags |= NETWORK_CONN_FLAG;
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

static JsonDocument doc;

void decodeMessage(const char* message) {
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Log.errorf(F("Unable to deserialize received JSON: %s\r\n"),
                   error.f_str());
        return;
    }

    // Handle command frame
    const char* cmd = doc["state"]["cmd"];
    if (cmd == 0) {
        Log.errorf(F("Unable to get command, pointer is zero, "
                     "message is %s\r\n"),
                   message);
        return;
    }

    // If it's a toggle_led command, handle it
    if (strcmp_P(cmd, PSTR("set_led")) == 0) {
        // -- Read the led value
        const char* target_led = doc["state"]["opts"]["led"];
        const unsigned int target_state =
            doc["state"]["opts"]["state"].as<unsigned int>();

        if (target_led == 0) {
            Log.errorf(F("Unable to get target led or state, pointer is zero, "
                         "message is %s\r\n"),
                       message);
            return;
        }

        // -- Toggle LED based on the given value
        Led led;

        if (strcmp_P(target_led, PSTR("USER")) == 0) {
            led = Led::USER;
        } else if (strcmp_P(target_led, PSTR("ERROR")) == 0) {
            led = Led::ERROR;
        } else {
            Log.errorf(F("Invalid LED value provided, "
                         "led provided = %s\r\n"),
                       target_led);
            return;
        }

        if (target_state) {
            Log.infof(F("Turning LED %s on\r\n"), target_led);
            LedCtrl.on(led);
        } else {
            Log.infof(F("Turning LED %s off\r\n"), target_led);
            LedCtrl.off(led);
        }

    } else if (strcmp_P(cmd, PSTR("stream")) == 0) {
        const unsigned int duration =
            doc["state"]["opts"]["duration"].as<unsigned int>();

        const unsigned int frequency =
            doc["state"]["opts"]["freq"].as<unsigned int>();

        if (duration == 0 || frequency == 0) {
            Log.errorf(F("Unable to get duration or frequency, pointer "
                         "is zero, message is %s\r\n"),
                       message);
            return;
        }

        data_frequency  = frequency;
        seconds_counted = 0;
        target_seconds  = duration;

        event_flags |= START_PUBLISHING_SENSOR_DATA_FLAG;
    } else if (strcmp_P(cmd, PSTR("verbose_logs")) == 0) {
        Log.setLogLevel(LogLevel::DEBUG);
    }
}

void printHelp() {
    Log.rawf(
        F("\r\nAvailable Commands\r\n"
          "-----------------------\r\n"
          "help\t\t Print this message\r\n"
          "loglevel=level\t Set the log level. Available levels are debug, "
          "info, warn, error\r\n"
          "-----------------------\r\n"));
}

void handleSerialCommand(const char* instruction, uint16_t instructionLen) {
    // Find the first occurrence of '='
    char* equalIndex = strchr(instruction, '=');

    // If we did not find it, treat is at a non-value command
    if (equalIndex == NULL) {
        equalIndex = (char*)(&instruction[instructionLen - 1]);
        Log.debug(F("Given command is non-value"));
    }

    // Extract the command
    uint16_t cmdLen = equalIndex - instruction;
    char cmd[cmdLen + 1];
    memcpy(cmd, instruction, cmdLen);
    cmd[cmdLen] = '\0';

    // Extract the value
    uint16_t valueLen = instructionLen - cmdLen - 1;
    char value[valueLen + 1];
    memcpy(value, instruction + cmdLen + 1, valueLen);
    value[valueLen] = '\0';

    // Depending on the cmd content, execute different commands
    if (strcmp_P(cmd, PSTR("help")) == 0) {
        printHelp();
    } else if (strcmp_P(cmd, PSTR("loglevel")) == 0) {
        if (!Log.setLogLevelStr(value)) {
            Log.errorf(F("Could not set log level %s\r\n"), value);
        } else {
            Log.rawf(F("Log level is now %s\r\n"), value);
        }
    } else if (strcmp_P(cmd, PSTR("heartbeat")) == 0) {
        event_flags |= SEND_HEARTBEAT_FLAG;
    } else if (strcmp_P(cmd, PSTR("reset")) == 0) {
        asm("jmp 0");
    } else {
        Log.info(F("\nInvalid command"));
        printHelp();
        return;
    }
}

void setup() {
    Log.begin(115200);

    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Set PD2 as input (button)
    pinConfigure(PIN_PD2, PIN_DIR_INPUT | PIN_PULLUP_ON);
    attachInterrupt(PIN_PD2, sendHeartbeatInterrupt, FALLING);

    // Set PF6 as input (reset button)
    pinConfigure(PIN_PF6, PIN_DIR_INPUT | PIN_PULLUP_ON);
    attachInterrupt(PIN_PF6, resetInterrupt, FALLING);

    sei();

    Log.infof(F("Starting sandbox / landing page procedure. Version = %s\r\n"),
              SANDBOX_VERSION);

    if (Mcp9808.begin()) {
        Log.error(F("Could not initialize the temperature sensor"));

        while (1) {}
    }

    if (Veml3328.begin()) {
        Log.error(F("Could not initialize the light sensor"));

        while (1) {}
    }

    ECC608.begin();

    // Find the thing ID and set the publish and subscription topics
    uint8_t thing_name[128];
    size_t thing_name_len = sizeof(thing_name);

    ATCA_STATUS status =
        ECC608.readProvisionItem(AWS_THINGNAME, thing_name, &thing_name_len);
    if (status != ATCA_SUCCESS) {
        Log.error(F("Could not retrieve thing name from the ECC"));
        Log.error(F("Unable to initialize the MQTT topics. Stopping..."));
        LedCtrl.on(Led::ERROR);
        return;
    }

    Log.infof(F("Board name: %s\r\n"), thing_name);

    sprintf_P(mqtt_sub_topic, MQTT_SUB_TOPIC_FMT, thing_name);
    sprintf_P(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, thing_name);

    Log.info(
        F("Will now connect to the operator. If the board hasn't previously "
          "connected to the operator/network, establishing the "
          "connection the first time might take some time."));
    connectLTE();
}

void loop() {

    // See if there are any messages for the command handler
    if (Serial3.available()) {
        String extractedString = Serial3.readStringUntil('\n');
        handleSerialCommand(extractedString.c_str(), extractedString.length());
    }

    // ----------------------------------------------------------
    if (event_flags & NETWORK_CONN_FLAG) {
        switch (state) {
        case NOT_CONNECTED:
            state = CONNECTED_TO_NETWORK;
            LedCtrl.on(Led::CELL);
            Log.infof(F("Connected to operator: %s\r\n"),
                      Lte.getOperator().c_str());
            connectMqtt();

            Log.infof(
                F("Connected to MQTT broker, subscribing to topic: %s!\r\n"),
                mqtt_sub_topic);
            MqttClient.subscribe(mqtt_sub_topic, AT_LEAST_ONCE);
            state = CONNECTED_TO_BROKER;
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

            Log.info(F("Network disconnection, attempting to reconnect..."));

            Lte.end();
            connectLTE();
            break;
        }

        event_flags &= ~NETWORK_DISCONN_FLAG;

    } else if (event_flags & BROKER_DISCONN_FLAG) {

        switch (state) {
        case CONNECTED_TO_BROKER:
            state = CONNECTED_TO_NETWORK;

            Log.info(
                F("Lost connection to broker, attempting to reconnect..."));

            connectMqtt();
            MqttClient.subscribe(mqtt_sub_topic, AT_LEAST_ONCE);

            break;

        case STREAMING_DATA:
            state = CONNECTED_TO_NETWORK;

            Log.info(
                F("Lost connection to broker, attempting to reconnect..."));

            stopStreamTimer();
            connectMqtt();
            MqttClient.subscribe(mqtt_sub_topic, AT_LEAST_ONCE);

            break;

        default:
            break;
        }

        event_flags &= ~BROKER_DISCONN_FLAG;
    } else if (received_message_identifiers_head !=
               received_message_identifiers_tail) {

        switch (state) {
        case CONNECTED_TO_BROKER:
        case STREAMING_DATA: {

            char message[384] = "";

            cli();
            received_message_identifiers_tail =
                (received_message_identifiers_tail + 1) &
                RECEIVE_MESSAGE_ID_BUFFER_MASK;

            const uint32_t message_id =
                received_message_identifiers[received_message_identifiers_tail];
            sei();

            const bool message_read_successfully = MqttClient.readMessage(
                mqtt_sub_topic,
                message,
                sizeof(message),
                message_id);

            if (message_read_successfully) {
                decodeMessage(message);
            } else {
                Log.error(F("Failed to read message\r\n"));
            }

        } break;

        default:
            break;
        }

    } else if (event_flags & SEND_HEARTBEAT_FLAG) {

        switch (state) {

        case CONNECTED_TO_BROKER:
        case STREAMING_DATA:

            Log.info(F("Sending heartbeat"));
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

            Log.infof(F("Starting to stream data for %d seconds\r\n"),
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
            sprintf_P(transmit_buffer,
                      PSTR("{\"type\": \"data\",\
                        \"data\": { \
                            \"Temperature\": %d, \
                            \"Light Intensity\": %d \
                        } \
                    }"),
                      int(Mcp9808.readTempC()),
                      Veml3328.getRed());

            if (!MqttClient.publish(mqtt_pub_topic, transmit_buffer)) {
                Log.errorf(F("Failed to publish message: %s\r\n"),
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

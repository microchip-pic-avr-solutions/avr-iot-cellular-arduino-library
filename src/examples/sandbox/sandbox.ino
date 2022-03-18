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

#define HEARTBEAT_INTERVAL_MS 60000
#define STREAM_DURATION       10000

// AWS defines which topic you are allowed to subscribe and publish too. This is
// defined by the policy The default policy with the Microchip IoT Provisioning
// Tool allows for publishing and subscribing on thing_id/topic. If you want to
// publish and subscribe on other topics, see the AWS IoT Core Policy
// documentation.
#define MQTT_SUB_TOPIC_FMT "$aws/things/%s/shadow/update/delta"
#define MQTT_PUB_TOPIC_FMT "%s/sensors"

char mqtt_sub_topic[128];
char mqtt_pub_topic[128];

volatile bool lteConnected = false;
volatile bool mqttConnected = false;
volatile bool mqttDataReceived = false;

volatile static unsigned long lastHeartbeatTime = 0;
static unsigned long lastDataFrame = 0;

static bool streamingEnabled = false;
static volatile uint16_t secondsCounted = 0;
static volatile uint16_t targetSecondCount = 0;
static volatile uint16_t dataFrequency = 0;

const char heartbeatMessage[] = "{\"type\": \"heartbeat\"}";

char transmitBuffer[256];

static char topic_buffer[MQTT_TOPIC_MAX_LENGTH + 1] = "";
static uint16_t message_length = 0;

ISR(TCA0_OVF_vect) {
    secondsCounted++;

    LedCtrl.toggle(Led::DATA);

    if (secondsCounted == targetSecondCount) {
        streamingEnabled = false;
        secondsCounted = 0;
        TCA0.SINGLE.CTRLA = 0;
        LedCtrl.off(Led::DATA);
        LedCtrl.off(Led::USER);
    }

    TCA0.SINGLE.INTFLAGS |= (1 << 0);
}

ISR(PORTD_PORT_vect) {
    if (PORTD.INTFLAGS && (1 << 2)) {
        lastHeartbeatTime = 0;
        PORTD.INTFLAGS |= (1 << 2);
    }
}

void mqttDCHandler() { mqttConnected = false; }
void mqttConHandler() { mqttConnected = true; }

void lteDCHandler() { lteConnected = false; }

void mqttDataHandler(char *topic, uint16_t msg_length) {
    digitalWrite(PIN_A2, HIGH);
    mqttDataReceived = true;
    memcpy(topic_buffer, topic, MQTT_TOPIC_MAX_LENGTH);
    message_length = msg_length;
}

void connectMqtt() {
    MqttClient.onConnectionStatusChange(mqttConHandler, mqttDCHandler);
    MqttClient.onReceive(mqttDataHandler);

    // Attempt to connect to broker
    MqttClient.beginAWS();

    Log.info("Connecting to AWS Sandbox...");
    while (!MqttClient.isConnected()) {
        Log.info("Connecting...");
        delay(500);

        // If we're not connected to the network, give up
        if (!lteConnected) {
            return;
        }
    }

    Log.info("Connected to AWS Sandbox!\r\n");
    // Subscribe to the topic
    MqttClient.subscribe(mqtt_sub_topic);
}

void connectLTE() {

    Lte.onConnectionStatusChange(NULL, lteDCHandler);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Not connected to Truphone yet...\r\n");
        delay(5000);
    }

    Log.info("Connected to Truphone!\r\n");
    lteConnected = true;
}

bool initMQTTTopics() {
    ECC608.begin();

    // Find the thing ID and set the publish and subscription topics
    uint8_t thingName[128];
    uint8_t thingNameLen = sizeof(thingName);

    // -- Get the thingname
    uint8_t err = ECC608.getThingName(thingName, &thingNameLen);
    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve thingname from the ECC");
        return false;
    }

    sprintf(mqtt_sub_topic, MQTT_SUB_TOPIC_FMT, thingName);
    sprintf(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, thingName);

    return true;
}

void startStreamTimer() {
    LedCtrl.on(Led::USER);

    // We need to tell the core that we're "taking over" the TCA0 timer
    takeOverTCA0();

    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;

    // Count per second (24MHz)
    TCA0.SINGLE.PER = 23437;

    sei();
    // Enable the timer with a division of 1024
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
}

void setup() {
    Log.begin(115200);
    Log.setLogLevel(LogLevel::INFO);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    // Set PD2 as input (button)
    pinMode(PIN_PD2, INPUT);
    PORTD.PIN2CTRL |= (0x3 << 0);

    pinMode(PIN_A2, OUTPUT);

    Log.infof("Starting sandbox / landing page procedure. Version = %s\r\n",
              SANDBOX_VERSION);

    if (mcp9808.begin() == -1) {
        Log.error("Could not initialize the temperature sensor");
    }

    if (initMQTTTopics() == false) {
        Log.error("Unable to initialize the MQTT topics. Stopping...");
        while (1)
            ;
    }

    connectLTE();
    connectMqtt();
}

void loop() {
loopFinished:

    if (mqttConnected) {

        // If it's more than HEARTBEAT_INTERVAL_MS since the last heartbeat,
        // send a heartbeat message
        if ((millis() - lastHeartbeatTime) > HEARTBEAT_INTERVAL_MS) {
            Log.info("Sending heartbeat");
            MqttClient.publish(mqtt_pub_topic, heartbeatMessage);
            lastHeartbeatTime = millis();
        }

        if (streamingEnabled) {
            if ((millis() - lastDataFrame) < (dataFrequency)) {
                goto receiveMessage;
            }
            lastDataFrame = millis();
            // Send data
            // -- Warning: Doing sprintf on a pointer without checking for
            // overflow is *bad* practice, but we do it here due to the
            // simplicity of the data.
            sprintf(transmitBuffer,
                    "{\"type\": \"data\",\
                    \"data\": { \
	\"Light\": %d, \
	\"Temperature\": %d \
    } \
	}",
                    5,
                    int(mcp9808.read_temp_c()));
            bool publishedSuccessfully =
                MqttClient.publish(mqtt_pub_topic, transmitBuffer);
            if (publishedSuccessfully != true) {
                Log.errorf("Could not publish message: %s\r\n", transmitBuffer);
            }
        }

    receiveMessage:
        if (mqttDataReceived) {
            char message[message_length + 16] = "";

            if (!MqttClient.readMessage(
                    topic_buffer, (uint8_t *)message, sizeof(message))) {
                Log.errorf("could not read mqtt message on topic %s\r\n",
                           topic_buffer);
            }

            mqttDataReceived = false;
            digitalWrite(PIN_A2, LOW);

            Log.debugf("new message: %s\r\n", message);
            StaticJsonDocument<400> doc;
            DeserializationError error = deserializeJson(doc, message);
            if (error) {
                Log.errorf("Unable to deserialize received JSON: %s\r\n",
                           error.f_str());
                goto loopFinished;
            }

            // Handle command frame
            const char *cmd = doc["state"]["cmd"];
            if (cmd == 0) {
                Log.errorf("Unable to get command, pointer is zero, "
                           "message is %s\r\n",
                           message);
                goto loopFinished;
            }

            // If it's a toggle_led command, handle it
            if (strcmp(cmd, "set_led") == 0) {
                // -- Read the led value
                const char *target_led = doc["state"]["opts"]["led"];
                const unsigned int target_state =
                    doc["state"]["opts"]["state"].as<unsigned int>();
                if (target_led == 0) {
                    Log.errorf(
                        "Unable to get target led or state, pointer is zero, "
                        "message is %s\r\n",
                        message);
                    goto loopFinished;
                }

                // -- Toggle LED based on the given value
                Led targetLed;

                if (strcmp(target_led, "USER") == 0) {
                    targetLed = Led::USER;
                } else if (strcmp(target_led, "ERROR") == 0) {
                    targetLed = Led::ERROR;
                } else {
                    Log.errorf("Invalid LED value provided, "
                               "led provided = %s\r\n",
                               target_led);
                    goto loopFinished;
                }

                if (target_state) {
                    Log.infof("Turning LED %s on\r\n", target_led);
                    LedCtrl.on(targetLed);
                } else {
                    Log.infof("Turning LED %s off (\%d = %d)\r\n",
                              target_led,
                              targetLed);
                    LedCtrl.off(targetLed);
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
                    goto loopFinished;
                }

                Log.infof("Starting to stream for %d s\r\n", duration);
                dataFrequency = frequency;
                secondsCounted = 0;
                targetSecondCount = duration;
                streamingEnabled = true;
                startStreamTimer();
            }
        }

    } else {
        // MQTT is not connected. Need to re-establish connection
        if (!lteConnected) {
            // We're NOT connected to the LTE Network. Establish LTE
            // connection first
            connectLTE();
        }

        connectMqtt();
    }
}

/**
 * This example is the code that runs during the initial landing page / sandbox
 * experience
 */

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

static unsigned long lastHeartbeatTime = HEARTBEAT_INTERVAL_MS;
static unsigned long lastDataFrame = 0;

static bool streamingEnabled = false;
static volatile uint16_t secondsCounted = 0;
static volatile uint16_t targetSecondCount = 0;
static volatile uint16_t dataFrequency = 0;

const char heartbeatMessage[] = "{\"type\": \"heartbeat\"}";

char transmitBuffer[256];

void mqttDCHandler() { mqttConnected = false; }

void lteDCHandler() { lteConnected = false; }

void connectMqtt() {
    MqttClient.onConnectionStatusChange(NULL, mqttDCHandler);

    // Attempt to connect to broker
    mqttConnected = MqttClient.beginAWS();

    if (mqttConnected) {
        Log.info("Connecting to broker...");
        while (!MqttClient.isConnected()) {
            Log.info("Connecting...");
            delay(500);

            // If we're not connected to the network, give up
            if (!lteConnected) {
                return;
            }
        }

        Log.info("Connected to broker!\r\n");

        // Subscribe to the topic
        MqttClient.subscribe(mqtt_sub_topic);
    } else {
        Log.error("Failed to connect to broker\r\n");
    }
}

void connectLTE() {

    Lte.onConnectionStatusChange(NULL, lteDCHandler);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();

    while (!Lte.isConnected()) {
        Log.info("Not connected to operator yet...\r\n");
        delay(5000);
    }

    Log.info("Connected to operator!\r\n");
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

    Log.info("Starting sandbox / landing page procedure\r\n");

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
            if ((millis() - lastDataFrame) < (dataFrequency * 1000)) {
                goto receiveMessage;
            }
            lastDataFrame = millis();
            // Send data
            // -- Warning: Doing sprintf on a pointer without checking for
            // overflow is *bad* practice, but we do it here due to the
            // simplicity of the loop.
            sprintf(transmitBuffer,
                    "{\"type\": \"data\",\
	\"x\": %d, \
	\"y\": %d \
	}",
                    secondsCounted,
                    int(mcp9808.read_temp_c()));
            bool publishedSuccessfully =
                MqttClient.publish(mqtt_pub_topic, transmitBuffer);
            if (publishedSuccessfully != true) {
                Log.errorf("Could not publish message: %s\r\n", transmitBuffer);
            }
            Log.infof("Sent data: %s\r\n", transmitBuffer);
        }

    receiveMessage:
        // Receieve message
        String message = MqttClient.readMessage(mqtt_sub_topic);
        if (message != "") {
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
                Log.errorf(
                    "Unable to get command, pointer is zero, message is %s\r\n",
                    message.c_str());
                goto loopFinished;
            }

            // If it's a toggle_led command, handle it
            if (strcmp(cmd, "toggle_led") == 0) {
                // -- Read the led value
                const char *target_led = doc["state"]["opts"];
                if (target_led == 0) {
                    Log.errorf("Unable to get target led, pointer is zero, "
                               "message is %s\r\n",
                               message.c_str());
                    goto loopFinished;
                }

                // -- Toggle LED based on the given value
                if (strcmp(target_led, "USER") == 0) {
                    LedCtrl.toggle(Led::USER);
                } else if (strcmp(target_led, "ERROR") == 0) {
                    LedCtrl.toggle(Led::ERROR);
                } else if (strcmp(target_led, "RAINBOW")) {
                    LedCtrl.startupCycle();
                } else {
                    Log.errorf("Invalid LED value provided, "
                               "led provided = %s\r\n",
                               target_led);
                    goto loopFinished;
                }
            } else if (strcmp(cmd, "stream") == 0) {
                const unsigned int duration =
                    doc["state"]["opts"]["duration"].as<unsigned int>();
                const unsigned int frequency =
                    doc["state"]["opts"]["frequency"].as<unsigned int>();
                if (duration == 0 || frequency == 0) {
                    Log.errorf("Unable to get duration or frequency, pointer "
                               "is zero, message is %s\r\n",
                               message.c_str());
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
            // We're NOT connected to the LTE Network. Establish LTE connection
            // first
            connectLTE();
        }

        connectMqtt();
    }
}

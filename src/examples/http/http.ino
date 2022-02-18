#include <Arduino.h>
#include <http_client.h>
#include <log.h>
#include <lte.h>
#include <sequans_controller.h>

#define DOMAIN "httpbin.org"

void testHttp();

void setup() {

    Log.begin(115200);
    Log.setLogLevel(LogLevel::DEBUG);

    // Start LTE modem and wait until we are connected to the operator
    Lte.begin();
    while (!Lte.isConnected()) {
        Log.info("Not connected to operator yet...\r\n");
        delay(1000);
    }

    Log.info("Connected!\r\n");

    testHttp();
}

void testHttp() {

    Log.info("---- Testing HTTP ----\r\n");

    HttpResponse response;

    // --- HTTP ---

    if (!HttpClient.configure(DOMAIN, 80, false)) {
        Log.info("Failed to configure http client\r\n");
    }

    Log.info("Configured to HTTP\r\n");

    response = HttpClient.post("/post", "{\"hello\": \"world\"}");
    Log.infof("POST - status code: %u, data size: %u\r\n",
              response.status_code,
              response.data_size);

    // --- HTTPS ---

    if (!HttpClient.configure(DOMAIN, 443, true)) {
        Log.info("Failed to configure https client\r\n");
    }

    Log.info("Configured to HTTPS\r\n");

    return;

    // TODO: This fails...
    response = HttpClient.head("/get");
    Log.infof("HEAD - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    response = HttpClient.get("/get");
    Log.infof("GET - status code: %d, data size: %d\r\n",
              response.status_code,
              response.data_size);

    String body = HttpClient.readBody(512);

    if (body != "") {
        Log.info("Body:\r\n");
        Log.raw(body);
    }
}

// ------------------------------ DEBUG BRIDGE ----------------------------- //

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    128
#define RESPONSE_BUFFER_SIZE 256

#ifdef __AVR_AVR128DB48__ // MINI

#define SerialDebug Serial3

#else
#ifdef __AVR_AVR128DB64__ // Non-Mini

#define SerialDebug Serial5

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

void debugBridgeUpdate(void) {
    static uint8_t character;
    static char input_buffer[INPUT_BUFFER_SIZE];
    static uint8_t input_buffer_index = 0;

    if (SerialDebug.available() > 0) {
        character = SerialDebug.read();

        switch (character) {
        case DEL_CHARACTER:
            if (strlen(input_buffer) > 0) {
                input_buffer[input_buffer_index--] = 0;
            }
            break;

        case ENTER_CHARACTER:
            SequansController.writeCommand(input_buffer);

            // Reset buffer
            memset(input_buffer, 0, sizeof(input_buffer));
            input_buffer_index = 0;

            break;

        default:
            input_buffer[input_buffer_index++] = character;
            break;
        }

        SerialDebug.print((char)character);
    }

    if (SequansController.isRxReady()) {
        // Send back data from modem to host
        SerialDebug.write(SequansController.readByte());
    }
}

void loop() { debugBridgeUpdate(); }

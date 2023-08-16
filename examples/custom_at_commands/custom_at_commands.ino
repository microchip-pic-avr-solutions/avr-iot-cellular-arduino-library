/**
 * @brief This example demonstrates how to use the SequansController to send AT
 * commands to the Sequans GM02S modem.
 */

#include <log.h>
#include <lte.h>
#include <sequans_controller.h>

#include <string.h>

static char ping_response[512] = "";

static volatile size_t ping_response_index = 0;

static volatile size_t ping_messages_received = 0;

static void ping_callback(char* message) {
    // We don't want to include a whitespace at the start of the string, so we
    // remove one from the length and move the pointer by one
    const size_t message_length = strlen(message) - 1;
    message                     = message + 1;

    // We store all the notification messsage data in a buffer where we adjust
    // the index of the data according to the length of the message
    //
    // We do messsage + 1 here to move the pointer so we don't include a
    // whitespace.
    memcpy(ping_response + ping_response_index, message, message_length);

    ping_response_index += message_length;

    // Append new line for every retrieved message
    ping_response[ping_response_index++] = '\r';
    ping_response[ping_response_index++] = '\n';

    ping_messages_received++;
}

void setup() {
    Log.begin(115200);

    Log.info(F("Starting up example for custom AT commands"));

    // If we didn't want to connect to the network, we could start the
    // SequansController directly by: SequansController.begin();
    // Lte.begin() will start the SequansController in its begin()
    Lte.begin();

    // Here we enable verbose error messages
    SequansController.writeCommand(F("AT+CMEE=2"));

    // Here we perform a ping with incorrect parameter in order to trigger the
    // error message. Note that if the modem returns an error with the command,
    // the SequansController will retry 5 times with an interval of 2 seconds,
    // so this will take 10 seconds
    //
    // Here we also pass an optional response buffer to the function which will
    // be filled with the response from the command
    char response[128]             = "";
    ResponseResult response_result = SequansController.writeCommand(
        F("AT+PING=0"),
        response,
        sizeof(response));

    if (response_result == ResponseResult::OK) {
        Log.infof(F("Command written successfully, this should not happen"));
    } else {
        Log.errorf(F("Error writing command, the response was: %s\r\n"),
                   response);
    }

    // --------------------- Notifications & Commands -------------------------

    // Now we're going to perform a ping to google in a slighly different way
    // and set up a notification so that we can inspect the result

    // First we set up a callback when the modem sends back an URC (unsolicited
    // response code), which can be though of as a notification.
    //
    // The different URCs are documented in Sequans' AT command reference.
    SequansController.registerCallback(F("PING"), ping_callback);

    // Instead of writing a command, we use the writeString function here. It
    // will simply write the string we provide and not check whether the command
    // was written successfully. We do it this way for this example as we want
    // to utilise notifications and the ping command is blocking. This is thus
    // purely an example.
    SequansController.writeString(F("AT+PING=\"www.microchip.com\""), true);

    // The default ping will retrieve four responses, so wait for them
    while (ping_messages_received < 4) {}

    Log.infof(F("Received the following ping response:\r\n%s\r\n"),
              ping_response);

    // -------------- Extracting Parameters from Responses --------------------

    // Here we will utilise AT+CEREG?, which returns data about the current
    // connection. We can use it to check if we are connected to the network.
    response_result = SequansController.writeCommand(F("AT+CEREG?"),
                                                     response,
                                                     sizeof(response));

    if (response_result == ResponseResult::OK) {

        Log.infof(F("Command written successfully, the response was: %s\r\n"),
                  response);

        char value_buffer[8] = "";

        // Extract the 1st index (zero indexed), which tells us about the
        // connection status
        if (SequansController.extractValueFromCommandResponse(
                response,
                1,
                value_buffer,
                sizeof(value_buffer))) {
            Log.infof(F("The value was: %s\r\n"), value_buffer);
        } else {
            Log.error(F("Failed to extract value"));
        }
    } else {
        Log.errorf(F("Error writing command, the response was: %s\r\n"),
                   response);
    }

    Lte.end();
}

void loop() {}
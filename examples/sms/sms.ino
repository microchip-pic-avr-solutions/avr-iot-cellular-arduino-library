/**
 * @brief This example demonstrates how to send SMS messages using the modem on
 * the AVR-IoT Cellular Mini.
 */
#include <Arduino.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <sequans_controller.h>

#define NUMBER "<FILL IN YOUR NUMBER WITH COUNTRY CODE, E.G., +4712345678>"

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.begin(115200);
    Log.rawf(F("\r\n\r\n"));
    Log.info(F("Starting SMS example. NOTE THAT THE TRUPHONE SIM CARD DOES NOT "
               "SUPPORT SENDING SMS TO OTHER NUMBERS THAN ITS IOT CONNECT HUB "
               "(NUMBER: 6260). USE ANOTHER SIM CARD WITH COMBINED DATA+SMS TO "
               "SEND SMS."));

    if (!SequansController.begin()) {
        Log.error(F("Failed to start the modem"));
        return;
    }

    char response[128] = "";

    // Configure UE for EPS combined attach
    ResponseResult response_result = SequansController.writeCommand(
        F("AT+CEMODE=2"),
        response,
        sizeof(response));

    if (response_result != ResponseResult::OK) {
        Log.errorf(F("Error writing combined attach command, the response was: "
                     "%s\r\n"),
                   response);
        return;
    }

    // --- Start modem and connect to the operator ---
    if (!Lte.begin()) {
        Log.error(F("Failed to connect to the operator"));
        return;
    }

    // --- Configure the UE to work under SMS text mode ---
    response_result = SequansController.writeCommand(F("AT+CMGF=1"),
                                                     response,
                                                     sizeof(response));

    if (response_result != ResponseResult::OK) {
        Log.errorf(F("Failed to configure the UE to work under SMS text mode, "
                     "the response was: %s\r\n"),
                   response);
        return;
    }

    // --- Query the SMS service centre address to configure it is already
    // correctly configured ---
    response_result = SequansController.writeCommand(F("AT+CSCA?"),
                                                     response,
                                                     sizeof(response));

    if (response_result != ResponseResult::OK) {
        Log.errorf(F("Failed to query the SMS service centre address, the "
                     "response was: %s\r\n"),
                   response);
        return;
    } else {
        char service_center_address[64] = "";

        if (!SequansController.extractValueFromCommandResponse(
                response,
                0,
                service_center_address,
                sizeof(service_center_address) - 1)) {
            Log.error(F("Failed to extract service center address"));
            return;
        }

        Log.infof(F("Got service centre addrees (this is not the SIM cards "
                    "number by the way): %s\r\n"),
                  service_center_address);
    }

    // --- Send SMS ---
    Log.infof(F("Sending SMS to %s...\r\n"), NUMBER);
    response_result = SequansController.writeCommand(
        F("AT+SQNSMSSEND=\"%s\",\"hello world\""),
        response,
        sizeof(response),
        NUMBER);

    if (response_result != ResponseResult::OK) {
        Log.errorf(F("Failed to send SMS, the response was: %s\r\n"), response);
        return;
    }

    SequansController.clearReceiveBuffer();

    Log.info(F("Waiting for send confirmation..."));
    if (!SequansController.waitForURC(F("SQNSMSSEND"),
                                      response,
                                      sizeof(response),
                                      40'000)) {
        Log.errorf(F("Timed out waiting for SMS confirmation\r\n"));
        return;
    }

    Log.infof(F("Got the following response: %s"), response);
}

void loop() {}

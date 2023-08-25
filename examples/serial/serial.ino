/**
 * @brief This file demonstrates how to use Serial for printing out messages on
 * the board. It also demonstrates how to use the logging module in the AVR-IoT
 * Cellular Library.
 */
#include <Arduino.h>

#include "log.h"

void setup() {

    // As can be seen on page 7 in the hardware user guide for the AVR-IoT
    // Cellular Mini:
    // https://ww1.microchip.com/downloads/en/DeviceDoc/AVR-IoT-Cellular-Mini-HW-UserGuide-DS50003320.pdf,
    // the USART/Serial connected to the debugger which is thereafter connected
    // to the USB port, is the Serial3. It is thus important that we use Serial3
    // and not Serial when printing messages.
    Serial3.begin(115200);
    Serial3.println("Hello world from Serial3");
    Serial3.end();

    delay(500);

    // The AVR-IoT Cellular Library also comes with a logging library, which
    // uses Serial3 under the hood and can be initialised in a similar way. It
    // will add a prefix to the particular logging level used (debug, info,
    // warning, error).
    Log.begin(115200);
    Log.info("Hello world from Log");

    // If no prefix is wanted, we can use raw:
    Log.raw("This is a message without a prefix");

    // The log level determines which messages are printed between the levels.
    // We can adjust the level as we want, the following will be printed under
    // the different levels:
    //
    // debug: All messages
    // info: info, warning and error messages
    // warn: Warning and error messages
    // error: Only error messages
    // none: No messages
    //
    // Note that raw messages always will be printed
    Log.setLogLevel(LogLevel::DEBUG);
    Log.debug("A debug message");
    Log.info("An info message");
    Log.warn("A warning message");
    Log.error("An error message");

    Log.setLogLevel(LogLevel::INFO);
    Log.raw(""); // Just to add a newline
    Log.debug("This will not be printed now");
    Log.info("An info message");
    Log.warn("A warning message");
    Log.error("An error message");

    Log.setLogLevel(LogLevel::WARN);
    Log.raw(""); // Just to add a newline
    Log.debug("This will not be printed now");
    Log.info("This will not be printed now");
    Log.warn("A warning message");
    Log.error("An error message");

    Log.setLogLevel(LogLevel::ERROR);
    Log.raw(""); // Just to add a newline
    Log.debug("This will not be printed now");
    Log.info("This will not be printed now");
    Log.warn("This will not be printed now");
    Log.error("An error message");

    Log.setLogLevel(LogLevel::NONE);
    Log.raw(""); // Just to add a newline
    Log.debug("This will not be printed now");
    Log.info("This will not be printed now");
    Log.warn("This will not be printed now");
    Log.error("This will not be printed now");
    Log.raw(""); // Just to add a newline

    Log.setLogLevel(LogLevel::INFO);

    // The logging library can also format strings by using the f postfix in the
    // function name. Note we have to add a \r\n for a carriage return and new
    // line when the format based functions are used.
    Log.infof("This is a number: %d\r\n", 10);
    Log.infof("This is a string: %s\r\n", "Hello world");
    Log.infof("This is a hexadecimal and a string: %X - %s\r\n",
              31,
              "Hello world");

    // The logging library also supports flash strings stored in program memory.
    // This is very useful for reducing memory usage
    Log.info(F("This is a flash string"));

    // The flash string functionality also support formatting
    Log.infof(F("This is a flash string with formatting: %d\r\n"), 10);
}

void loop() {}
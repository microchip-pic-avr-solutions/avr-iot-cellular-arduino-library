# 1.3.10

## Chages
* Use `JsonDocument` instead of the deprecated `StaticJsonDocument` from ArduinoJson in `sandbox.ino` and `plant_monitoring.ino`
* Correct some compiler warnings and typos


# 1.3.9

## Changes
* `mqtt_azure.ino` example sketch now uses callbacks for receiving messages, so that the wilcard subscription is handled properly


# 1.3.8

## Features

* Azure support with accompanying example sketch and provisioning
* Flash string support for the log module and the sequans controller with the `F()` macro
* A plant monitoring system example sketch is now included in the library
* The HttpClient now also reports curl status codes for better error handling

## Optimizations 

* The library now has a significantly lower RAM usage due to use of strings stored in flash rather than in RAM

## Changes

* `MqttClient.begin` is now blocking (within the timeout) and the callback registered with `MqttClient.onConnect` is deprecated and won't be called. This is to be able to capture error messages during the MQTT connection.

## Bugfixes

* The provisioning sketch now supports a chain of certificates
* Fixes some missing dependencies for the examples 


# 1.3.7

## Bugfixes
* Fix a bug where a disconnect from the network whilst being connected to a MQTT broker would cause the board to not respond 

# 1.3.6

## Features
* Add timeout parameter for HTTP(S)

## Changes
* LEDs toggle during HTTP queries (if the LED controller is not set to manual mode)
* Correct the certificates extacted from `extract_certificates.ino` so that they have the correct authority key identifier 
* Cryptoauthlib is now in its own folder in `src` to make the source folder more structured 

## Bugfixes
* Fix a bug where the device would not enter deep sleep due to the use of millis()
* Fix a bug where the interrupt flag for the reset button was cleared after the device was reset in `sandbox.ino`
* Fix a bug where pull-ups weren't enabled for some instances of SW0 and SW1 in the examples

## Deprecation notice
* This release deprecates the use of `ECC608.getThingName(buffer, &size)` and `ECC608.getEndpoint(buffer, &size)`. Use `ECC608.readProvisionItem(type, buffer, &size)` instead. The `type` is an enum of `ecc_data_types` found in `ecc608.h`. To e.g. read the AWS thing name, use: `ECC608.readProvisionItem(AWS_THINGNAME, buffer, &size)`.

# 1.3.5

## Changes
* A CA is now mandatory for provisioning
* The library now uses the latest version of cryptoauthlib

## Bugfixes
* Fix a bug where the device would not go in deep sleep when using the low power modes
* Fix a bug where a certificate would not be parsed correctly during provisioning

# 1.3.4

## Features
* Add a custom provisioning sketch for MQTT(S) and HTTP(S)
* Reduce power consumption in low power from ~300uA to ~70uA
* Add examples sketches for serial, GPIO and robust MQTT with connection loss

## Changes
* Improved MQTT URC handling
* Introduce an adjustable timout for Lte.begin()

## Bugfixes
* Fix a bug where the reset pin was floating in the sandbox application
* Fix a bug where reading the response in the SequansController would wrongly return an buffer overflow

# 1.3.3

## Features 
* Add ability to add HTTP headers 
* Add ability for specifying content type for HTTP POST requests

## Changes
* Security profile patch is removed as this is now taken care of by NTP synchronization
* Add Mqtt.end() in MQTT examples to prevent the broker waiting after running the example

## Bugfixes
* Fix a bug where Sequans.waitForURC would return true even when the URC was not received

# 1.3.2

Internal build



# 1.3.1

## Changes
* Turn off more modules in low power to save more power
* Add hardware-in-the-loop testing

## Bugfixes
* Fix a bug where it would take some time before the board was actually in power down mode
* Fix a bug where waking up from power down mode would reset the board
* Fix a bug where it was wrongly reported that the SIM card was not ready



# 1.3.0

## Features

* Add username and password authentication for MQTT
* Add example using username and password authentication for MQTT
* Add a function for retrieving the supply voltage of the board in LowPower and an example demonstrating it

## Changes

* Make the AT command system for the cellular modem faster and more robust
* Make cryptoauthlib not precompiled in order to reduce flash and RAM usage
* Reduce buffer sizes and function count (mostly callbacks) in the library to reduce total flash and RAM usage
* Change pin configuration for low power such that a lower current consumption is achieved

## Bugfixes

* Fix a bug where the NTP sync would not retry if it failed
* Fix a bug where the NTP sync would retry if cellular network got disconnected while doing the sync
* Fix a bug where the LEDs weren't returned to the previous state after a power save 


# 1.2.3

## Features

* Add GPS tracker example

## Changes 

* Update MCP9808 library name in library.properties dependency field
* Update VEML3328 library name in library.properties dependency field
* Update Jenkins pipeline with new arduino-lint configurations

## Bugfixes

* Fix a bug where the peripherals weren't powered up before the LTE module in power down mode
* Fix a bug where the LEDs wouldn't blink after using power down mode


# 1.2.2

No changes

# 1.2.1

## Changes

* Update library name in library.properties file

## Bugfixes

* Fix dependency error in library.properties file



# 1.2.0

## Features

* Split HTTP example into a HTTP and HTTPS example 
* Add a log message when HTTPS security profile is not set up
* Update library.properties to pass lint checks for PR with library-registry 
* Improve error messages for MQTT

## Changes 

* Use Arduino interrupt system so that the library doesn't hijack interrupt service routines used by other libraries 
* Change sandbox application to use increased MQTT Quality of Service in order to not lose received messages
* Remove MqttClient.disconnect() (full functionality given in MqttClient.end())

## Bugfixes

* Fix a bug where HTTP requests would make the modem hang
* Add a temporary fix for a bug where the modem would not respond during MQTT publish and receive
* Fix a bug where retrieving the modem clock would fail



# 1.1.3

## Bugfixes

* Fixes a bug where the CELL LED wouldn't blink while the device was connecting to the network
* Fixes a bug in the sandbox where some event flags could be cleared before being processed




# 1.1.2

## Features
* Add NTP synchronization in Lte.begin() if modem clock is not set

## Bugfixes
* Fixes a bug for TLS certificate verification in certain regions
* Fixes a bug where the power save functionality would not terminate
* Reduce time used for querying for operator



# 1.1.1

## Bugfixes

* Fixes a bug where LTE connectivity in sandbox was not reconnecting
* Added temporarily work around for TLS certificate verification in certain regions

# 1.1.0

## Features

* Added sensor driver shutdown to lower power examples
* Refactored sandbox to use modulo instead of masking
* Added a simple CLI for the sandbox

## Bugfixes

* Fixed a bug where the board buttons were disabled during sleep, and could not wake up the MCU
* Removed configuring button as input again after not disabling it
* Fixed a bug where flow control was not correctly applied when the receive buffer was full. This particularly affected the sandbox. 


# 1.0.0

* Initial Release


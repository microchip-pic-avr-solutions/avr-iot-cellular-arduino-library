# 1.0.0

* Initial Release

# 1.1.0

## Features

* Added sensor driver shutdown to lower power examples
* Refactored sandbox to use modulo instead of masking
* Added a simple CLI for the sandbox

## Bugfixes

* Fixed a bug where the board buttons were disabled during sleep, and could not wake up the MCU
* Removed configuring button as input again after not disabling it
* Fixed a bug where flow control was not correctly applied when the receive buffer was full. This particularly affected the sandbox. 

# 1.1.1

## Bugfixes

* Fixes a bug where LTE connectivity in sandbox was not reconnecting
* Added temporarily work around for TLS certificate verification in certain regions

# 1.1.2

## Features
* Add NTP synchronization in Lte.begin() if modem clock is not set

## Bugfixes
* Fixes a bug for TLS certificate verification in certain regions
* Fixes a bug where the power save functionality would not terminate
* Reduce time used for querying for operator

# 1.1.3

## Bugfixes

* Fixes a bug where the CELL LED wouldn't blink while the device was connecting to the network
* Fixes a bug in the sandbox where some event flags could be cleared before being processed

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

# 1.2.1

## Changes

* Update library name in library.properties file

## Bugfixes

* Fix dependency error in library.properties file

# 1.2.2

No changes

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

# 1.3.1

## Changes
* Turn off more modules in low power to save more power
* Add hardware-in-the-loop testing

## Bugfixes
* Fix a bug where it would take some time before the board was actually in power down mode
* Fix a bug where waking up from power down mode would reset the board
* Fix a bug where it was wrongly reported that the SIM card was not ready

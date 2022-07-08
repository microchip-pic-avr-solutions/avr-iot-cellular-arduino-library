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

## Bugfixes

* Fix a bug where HTTP requests would make the modem hang
* Fix a bug where retrieving the modem clock would fail

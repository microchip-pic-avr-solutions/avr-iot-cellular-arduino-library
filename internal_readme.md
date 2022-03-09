# Internal to Microchip (should probably not be included in a github readme)

## Setup required for development of the library

1. Download Arduino IDE and grab [DxCore](https://github.com/SpenceKonde/DxCore/blob/master/Installation.md).
2. Clone this repo to one's Arduino library [folder](https://www.arduino.cc/en/hacking/libraries) (Usually `Documents\Arduino\libraries` on Windows): `git clone --recursive https://bitbucket.microchip.com/scm/mcu8mass/avr-iot-cellular-arduino-firmware.git` 
3. Build cryptoauthlib archive and place headers in `src` folder with the command: `./scripts/inject_cryptoauthlib.sh` (The `./scripts/clear_cryptoauthlib.sh` removes all the cryptoauthlib related files from the source directory). *Note that this depends on make, cmake, gcc-avr and avr-libc*. This is a somewhat awkward setup, but we do this because of three things:
    - Arduino doesn't allow us to specify include paths from the library (it's fixed at the source folder), so we have to 'inject' the headers from cryptoauthlib in the source folder and not some sub folder.
    - Compile time is reduced significantly by using an archive for cryptoauthlib. There are a lot of source files in the cryptoauthlib, and having them be compiled each time the user uploads the sketch will slow down development for the users (especially in the Arduino IDE on Windows for some reason, compiling through arduino-cli within wsl was a lot quicker).
    - Easier to use for the user. In this way, the library can just be downloaded and used. **We only need to make sure that we do the 'injecting' before we create a new release.**
4. Provision your board with the iot-provisioning tool. 
5. Open up one of the examples in `src/examples` in the Arduino IDE. Modify your setup from the `tools` menu in Arduino IDE to set the board, chip and port and upload the sketch. 


## Provisioning: security profiles and certificates

The locations are in the Sequans' non-volatile memory if not specified otherwise.

| Profile          | CA location | Device certificate location | Private key location | Signature algorithm     | AT command                                |
|------------------|-------------|-----------------------------|----------------------|-------------------------|-------------------------------------------|
| 1. MQTT ECC      | 19          | 0                           | ECC slot 0           | ECDSA-with-SHA256       | AT+SQNSPCFG=1,2,"0xC02B",1,19,0,0,"","",1 |
| 2. MQTT NON-ECC  | 18          | 5                           | 5                    | sha256WithRSAEncryption | AT+SQNSPCFG=2,2,"0xC02F",1,18,5,5,"","",0 |
| 3. HTTPS         | 1           | N/A                         | N/A                  | N/A                     | AT+SQNSPCFG=3,2,"",1,1                    |


## Project structure

- `lib` is where cryptoauthlib lives, we have a custom CMakeLists.txt file which builds it to our preference, the atca config and device specification for the AVR128DB64.
- `src` is where all the source code, headers and examples are. The archive file for cryptoauthlib also has to be placed at src/<mcu target>/libcryptoauthlib.a. This is done by the inject script though, which also places all the other headers for cryptoauthlib in the src folder.
- `scripts` has the convenience scripts for building and injecting cryptoauthlib: `inject_cryptoauthlib.sh` and clearing it from the src folder: `clear_cryptoauthlib.sh`. There are also two other convenience scripts: `make.sh` and `flash.sh` (they've only been used during development and can be deleted).

## Code structure

Most modules in this library mimicks the Arduino class style, which is a somewhat frankenstein class pattern which acts more like a regular C module. They are singletons (or externs) and not instantiated by the user. Examples of this is: `Lte.begin()` and `HttpClient.get("/test")`, where Lte and HttpClient are instances of a LteClass and HttpClientClass. We have the dot notation and some encapsulation, but that's more or less what there is to this 'class' pattern. Besides that we use static functions since we have to for ISRs (only for Sequans Controller), so the rest of the compilation unit is more or less a regular C module. This decision was made to make things similar to how Arduino does them and to comply with their style guide for APIs, which just is what it is. A simple C module pattern would've done more than fine, but in this way it is at least familiar to people who've been programming a fair amount in an arduino environment.

There are four main modules:
- MQTT client (wrapper around MQTT functionality)
- HTTP client (wrapper around HTTP(s) functionality)
- LTE (sends AT commands for network)
- Sequans Controller (AT driver more or less)

The MQTT, HTTP and LTE modules are one step over the Sequans Controller in the abstraction layer, and just sends AT commands to the Sequans Controller. The library is both interrupt and polling driven (besides HTTP). Callbacks can be registered for MQTT and LTE on when connection has been made, messages has been received and so on. These callbacks are registered through the Sequans Controller by listening to certain URCs, and there are functions for registering more of them if necessary. 

There is one support file for cryptoauthlib, `src/hal_i2c_driver.cpp`, which is the hardware abstraction layer for I2C. There are also some examples in `src/examples`.


# Todo

## ECC
- There is some contion with hal_i2c where the if we don't use enough 
  time, it just halts. Need the delay for now.

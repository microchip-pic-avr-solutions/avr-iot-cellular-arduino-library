# Internal to Microchip (should probably not be included in a github readme)

## Setup required for development of the library

1. Download Arduino IDE and grab [DxCore](https://github.com/SpenceKonde/DxCore/blob/master/Installation.md).
2. Clone this repo to one's Arduino library [folder](https://www.arduino.cc/en/hacking/libraries) (Usually `Documents\Arduino\libraries` on Windows): `git clone --recursive https://bitbucket.microchip.com/scm/mcu8mass/avr-iot-cellular-arduino-firmware.git` 
3. Build cryptoauthlib archive and place headers in `src` folder: `./scripts/inject_cryptoauthlib.sh` (The `./scripts/clear_cryptoauthlib.sh` removes all the cryptoauthlib related files from the source directory). Note that this depends on make and cmake. This is a somewhat awkward setup, but we do this because of three things:
    - Arduino doesn't allow us to specify include paths from the library (it's fixed at the source folder), so we have to 'inject' the headers from cryptoauthlib in the source folder and not some sub folder.
    - Compile time is reduced significantly by using an archive for cryptoauthlib. There are a lot of source files in the cryptoauthlib, and having them be compiled each time the user uploads the sketch will slow down development for the users (especially in the Arduino IDE on Windows for some reason, compiling through arduino-cli within wsl was a lot quicker).
    - Easier to use for the user. In this way, the library can just be downloaded and used. **We only need to make sure that we do the 'injecting' before we create a new release.**
4. Provision your board **todo more here**. The board needs to be provisioned, which there are some details about below in the provisioning section, as well as on confluence.
5. Open up one of the examples in `src/examples` in the Arduino IDE. Modify your setup from the `tools` menu to set the board, chip and port and upload the sketch. If it complains about TWI1, look below.


## Things which need to be merged into DxCore

- This might have changed now, but currently there are no TWI1 support in DxCore. This is begin worked on in one of the issues and should be patched in soon. For making this to work at the moment you need to copy the contents of the patch in this [issue](https://github.com/SpenceKonde/DxCore/issues/54#issuecomment-860186363) by MX682X. Copy his source files to `<place where DxCore is located>/DxCore/hardware/megaavr/x.x.x/libraries/Wire/src`.
- Static linking support, this is merged into master of DxCore, but not upstream in a version yet. The PR is [here](https://github.com/SpenceKonde/DxCore/pull/128). If it is not yet upstream, you need to replace the platform.txt in that PR with the one in DxCore's root for cryptoauthlib to link correctly.


## Provisioning: security profiles and certificates

The locations are in the Sequans' non-volatile memory if not specified otherwise.

| Profile          | CA location | Device certificate location | Private key location | Signature algorithm     | AT command                                |
|------------------|-------------|-----------------------------|----------------------|-------------------------|-------------------------------------------|
| 1. MQTT ECC      | 19          | 0                           | ECC slot 0           | ECDSA-with-SHA256       | AT+SQNSPCFG=1,2,"0xC02B",1,19,0,0,"","",1 |
| 2. MQTT NON-ECC  | 18          | 5                           | 5                    | sha256WithRSAEncryption | AT+SQNSPCFG=2,2,"0xC02F",1,18,5,5,"","",0 |
| 3. HTTPS         | 1           | N/A                         | N/A                  | N/A                     | AT+SQNSPCFG=3,2,"",1,1                    |

For secure MQTT with TLS, we can utilise the ATECC. In order to get this communication to work we have to extract the certificate already stored on the ATECC and add them to the cloud provider/MQTT broker. The procedure is:

1. Extract device certificate (specific for ATECC608B and ATCECC508A). There is example code for this under the example folder, which will grab both the device certificate and signer certificate, though is only the device certificate needed. 
2. Register the device certificate in the sequans module and for the broker. `AT+SQNSNVW="certificate",0,<certificate length><CR><LF><certificate data>` for storing the device certificate in the sequans module. For registering with the broker this is different from broker to broker, but for AWS this is done in the IoT console (google is your friend here).
3. Store CA certificate for the MQTT broker on the Sequans module in slot given above, 19. This can be done with the `AT+SQNSNVW="certificate",19,<certificate length><CR><LF><certificate data>` command.
4. Enable this configuration with the AT command in the MQTT ECC row. Have a look at Seqans' AT command reference for more details around this command. 
5. Optional: If we want to use MQTT without the ECC we can enable a separate security profile at place 2 with the AT command for MQTT NON-ECC in the table above, but then certificates has to be loaded into the sequans modem for this. This is regular certificates which are generated in the AWS console when onboarding a device there as well as its root certificate which is downloaded when one runs the connect package script (gets downloaded from AWS). Also note that we use place 5 for certificate and private key and 18 for CA. We use place 5 since 1-4 are occupied with preloaded CA certificates.


For HTTPS we just use the bundled CA certificate(s) stored in the Sequans module. To make the security profile, issue the AT command in the table under the HTTPS row. We use the CA at location 1, which is preloaded Verisign certificate which comes with the modem. 



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


# Public things (outline for the GitHub readme)
 
## Setup (not finished yet, but this could be the user experience once everything we need is merged into DxCore and provisioning is done).

1. Provision your board against your cloud provider **todo: this will be done via iot-provisioning, so need those details her**
2. Download the Arduino IDE and grab [DxCore](https://github.com/SpenceKonde/DxCore/blob/master/Installation.md).
3. Download the latest release for of this library in the releases page on the right side. Open up the Arduino IDE and click `Sketch > Include Library > Add .ZIP library...`  and find where you downloaded the library.

## Using the library

The library can be used both in a polling fashion and an interrupt/callback based way when it comes to LTE connection and MQTT.

Have a look at the examples [mqtt_polling.ino](src/examples/mqtt_polling/mqtt_polling.ino), [mqtt_interrupt.ino](src/examples/mqtt_interrupt/mqtt_interrupt.ino) and [http.ino](src/examples/http/http.ino).




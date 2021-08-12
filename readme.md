# Not internal things

## Setup (not finished yet, but this could be the user experience once everything we need is merged into DxCore and provisioning is done).

1. Provision your board against your cloud provider **todo: need more details here**
2. Download the Arduino IDE and grab [DxCore](https://github.com/SpenceKonde/DxCore/blob/master/Installation.md).
3. Download the latest release for of this library in the releases page on the right side. Open up the Arduino IDE and click `Sketch > Include Library > Add .ZIP library...`  and find where you downloaded the library.

## Using the library

The library can be used both in a polling fashion and an interrupt/callback based way. There are examples of both for MQTT and HTTP(S).

### Connecting to Amazon Web Services (AWS), Microsoft Azure or Google Cloud

Have a look at the example [mqtt_polling.ino](src/examples/mqtt_polling/mqtt_polling.ino) and [mqtt_interrupt.ino](src/examples/mqtt_interrupt/mqtt_interrupt.ino).

### Using HTTP(S)

Have a look at the example [http_polling.ino](src/examples/http_polling/http_polling.ino) and [http_interrupt.ino](src/examples/http_interrupt/http_interrupt.ino)



# Internal to Microchip (should probably not be included in a github readme)

## Setup required for development of the library

Firstly, grab [DxCore](https://github.com/SpenceKonde/DxCore/blob/master/Installation.md).

Thereafter, one has to clone this repo to one's Arduino library [folder](https://www.arduino.cc/en/hacking/libraries). **Remember to use the --recursive flag during clone, as cryptoauthlib is a submodule**

For cryptoauthlib, we need build the archive, copy it and the header files to the `src` folder. This is done by calling the `./scripts/inject_cryptoauthlib.sh` script. The `./scripts/clear_cryptoauthlib.sh` removes all the cryptoauthlib related files from the source directory. This is a somewhat awkward setup, but we do this because of three things:

1. Arduino doesn't allow us to specify include paths from the library (it's fixed at the source folder), so we have to 'inject' the headers from cryptoauthlib in the source folder and not some sub folder.
2. Compile time is reduced significantly by using an archive for cryptoauthlib. There are a lot of source files in the cryptoauthlib, and having them be compiled each time the user uploads the sketch will slow down development for the users (especially in the Arduino IDE on Windows for some reason, compiling through arduino-cli within wsl was a lot quicker).
3. Easier to use for the user. In this way, the library can just be downloaded and used. We only need to make sure that we do the 'injecting' before we create a new release.

The scripts have dependencies on bash, cmake and make. So those need to be installed.

The board needs to be provisioned, which there are some details about below in the security profile and certificates section.

After that, one can run one of the examples in `src/examples`. Open one of them up in the Arduino IDE, modify your setup from the `tools` menu to set the board, chip and port and upload the sketch. If it complains about TWI1, look below.


### Things which need to be merged into DxCore

- This might have changed now, but currently there are no TWI1 support in DxCore. This is begin worked on in one of the issues and should be patched in soon. For making this to work at the moment you need to copy the contents of the patch in this [issue](https://github.com/SpenceKonde/DxCore/issues/54#issuecomment-860186363) by MX682X. Copy his source files to `<place where DxCore is located>/DxCore/hardware/megaavr/x.x.x/libraries/Wire/src`.
- Static linking support, hopefully this will be merged when this goes into someone other's hands. The PR is [here](https://github.com/SpenceKonde/DxCore/pull/128). If it is not yet upstream, you need to replace the platform.txt in that PR with the one in DxCore's root for cryptoauthlib to link correctly.


## Security profile and certificates

### MQTT 

For secure MQTT with TLS, we utilise the ATECC. In order to get this communication to work we have to extract the certificate already stored on the ATECC and add them to the cloud provider/MQTT broker. The procedure is:

1. Extract device certificate (specific for ATECC608B and ATCECC508A)

There is example code for this under the example folder, which will both the device certificate and signer certificate, which can be used. It grabs the public key by using atcab_getpubkey. Then it grabs the certificates by using atcacert_read_cert. Grabbing the certificate requires a certificate definition which can be found in the example folder. The certificate is in x509 format and can after extraction be uploaded to the MQTT broker. 

2. Register the device certificate in the sequans module and for the broker. `AT+SQNSNVW="certificate",0,<certificate length><CR><LF><certificate data>` for storing the device certificate in the sequans module. For registering with the broker this is different from broker to broker, but for AWS this is done in the IoT console (google is your friend here).  **might also need to register signer certificate, not sure about this**.

3. Store CA certificate for the MQTT broker on the Sequans module in slot 19 (arbitrary, but this is what the library uses). This can be done with the `AT+SQNSNVW="certificate",19,<certificate length><CR><LF><certificate data>` command.

4. Enable this configuration: `AT+SQNSPCFG=1,2,"0xC02B",5,19,0,0,"","",1`. Have a look at Seqans' AT command reference for more details around this command. 

5. Optional: If we want to use MQTT without the ECC we can enable a separate security profile at place 2: `AT+SQNSPCFG=2,2,"0xC02F",5,18,5,5,"","",0`, but then certificates has to be loaded into the sequans modem for this. Note that we use aes here. This is regular certificates which are generated in the AWS console when onboarding a device there. Also note that we use place 5 for certificate and private key and 18 for CA.


### HTTPS 

For HTTPS we just use the bundled CA certificate(s) stored in the Sequans module. To make the security profile, issue the following: `AT+SQNSPCFG=3,,,5,2`. We utilise security profile ID 2 for HTTPS, since we reserve 1 to MQTT. We also use the CA at location 2, which is preloaded digicert certificate which comes with Sequans module. **todo: should we use this or another CA for http?**





# Todo

## ECC
- There is some contion with hal_i2c where the if we don't use enough 
  time, it just halts. Need the delay for now.


## HTTPS
- Some hostnames still doesn't work... Might be something with CA


## MQTT
- Ability to not use ATECC? -> Store private key on Sequans module 

## Code

- All the todos in code
- Test the examples
- Test http aginast httpbin.org?

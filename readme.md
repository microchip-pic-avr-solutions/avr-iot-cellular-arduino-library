# Security profile and certificates

## MQTT 

For secure MQTT with TLS, we utilise the ATECC. In order to get this communication to work we have to extract the certificate already stored on the ATECC and add them to the cloud provider/MQTT broker. The procedure is:

1. Extract device certificate (specific for ATECC608B and ATCECC508A)

There is example code for this under the example folder, which will both the device certificate and signer certificate, which can be used. It grabs the public key by using atcab_getpubkey. Then it grabs the certificates by using atcacert_read_cert. Grabbing the certificate requires a certificate definition which can be found in the example folder. The certificate is in x509 format and can after extraction be uploaded to the MQTT broker. 

2. Register the device certificate. This is different from broker to broker, but for AWS this is done in the IoT console (google is your friend here). 

3. Store CA certificate for the MQTT broker on the Sequans module in slot 19 (arbitrary, but this is what the library uses). This can be done with the `AT+SQNSNVW="certificate",19,<certificate length><CR><LF><certificate data>` command.

4. Enable this configuration: `AT+SQNSPCFG=1,2,"0xC02B",5,19,0,0,"","",1`. Have a look at Seqans' AT command reference for more details around this command. 


## HTTPS 

For HTTPS we just use the bundled CA certificate(s) stored in the Sequans module. To make the security profile, issue the following: `AT+SQNSPCFG=2,,,5,1`. We utilise security profile ID 2 for HTTPS, since we reserve 1 to MQTT.

# Todo

## ECC
- There is some race condition with hal_i2c where the if we don't use enough 
  time, it just halts. Need the delay for now.


## HTTPS
- Some hostnames still doesn't work... Might be something with CA


## MQTT
- Ability to not use ATECC?


## Sequans Controller
- Static methods in Sequans controller and the ISR with the class structure
    - Could have some sort of singleton with private constructor


## General
- Strings in progmem?
- How do we enforce that there should be only one Mqtt client and http client?
    - For Http we could in fact have more than 1 using profiles, which would be 
      neat


## DxCore 
- Need extra flag for linking towards libcryptoauth.a, set as 
  compiler.extra_archive_paths for the moment
- I2C block on address = 0
- I2C: Test with another Dx for multi master and look at pins

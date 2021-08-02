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

/**
 * @brief Compilation unit for implementation of HAL i2c interface declared.
 * We have defined in the atca_config.h that we use i2c as the interface
 * for communicating with the ECC. The file atca_hal.h declares these functions
 * for i2c communication, but we have to provide the implementation for them.
 * Thus we have to have these functions here.
 */
#include "cryptoauthlib.h"

// TODO: Temp, just for linting
#define __AVR_AVR128DB64__

#include <Arduino.h>
#include <Wire.h>

#define WireECC         Wire1
#define SDA_PIN         PIN_PB2
#define SCL_PIN         PIN_PB2
#define AVR_I2C_ADDRESS 0x2

ATCA_STATUS hal_i2c_init(ATCAIface iface, ATCAIfaceCfg *cfg) {

    // TODO: This should be done with Wire1.swap(2) when it is implemented into
    // the core
    pinConfigure(SDA_PIN, PIN_PULLUP_ON);
    pinConfigure(SCL_PIN, PIN_PULLUP_ON);
    PORTMUX.TWIROUTEA =
        (PORTMUX.TWIROUTEA & !PORTMUX_TWI1_gm) | PORTMUX_TWI1_ALT2_gc;

    WireECC.setClock(cfg->atcai2c.baud);
    WireECC.begin();

    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_post_init(ATCAIface iface) { return ATCA_SUCCESS; }

ATCA_STATUS hal_i2c_send(ATCAIface iface,
                         uint8_t word_address,
                         uint8_t *txdata,
                         int txlength) {

    WireECC.beginTransmission(word_address);

    // Custom implementation of writing n bytes since the one provided from the
    // Wire library doesn't take failing to send a single byte into
    // consideration
    size_t index = 0;
    while (index < txlength) {
        if (WireECC.write(txdata[index])) {
            index++;
        }
    }

    WireECC.endTransmission();

    // The Wire interface blocks and checks the TWIx.MSTATUS flag for WIF, which
    // give us the indication that the transmit was completed, so we return
    // success here
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_receive(ATCAIface iface,
                            uint8_t word_address,
                            uint8_t *rxdata,
                            uint16_t *rxlength) {

    delay(100);

    // TODO: Somehow, the TWI driver gets into an infinite loop if we don't
    // delay some here. This might be due to two operations happening quickly
    // after each other. Two reads for example.
    // This is really bad though :/
    // delay(100);

    *rxlength = Wire1.requestFrom(word_address, (size_t)(*rxlength));

    int value;
    size_t i = 0;

    while (i < *rxlength) {
        value = Wire1.read();

        if (value != -1) {
            rxdata[i] = (uint8_t)value;
            i++;
        }
    }

    return ATCA_SUCCESS;
}

ATCA_STATUS
hal_i2c_control(ATCAIface iface, uint8_t option, void *param, size_t paramlen) {
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS hal_i2c_release(void *hal_data) {
    Wire1.end();
    return ATCA_SUCCESS;
}

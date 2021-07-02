/**
 * @brief Compilation unit for implementation of HAL i2c interface declared.
 * We have defined in the atca_config.h that we use i2c as the interface
 * for communicating with the ECC. The file atca_hal.h declares these functions
 * for i2c communication, but we have to provide the implementation for them.
 * Thus we have to have these functions here.
 */
#include "cryptoauthlib.h"

#include <Arduino.h>
#include <UART.h>
#include <Wire.h>

#define WireECC         Wire1
#define AVR_I2C_ADDRESS 0x2

ATCA_STATUS hal_i2c_init(ATCAIface iface, ATCAIfaceCfg *cfg) {

    WireECC.usePullups();
    WireECC.begin(cfg->atcai2c.address);
    WireECC.setClock(cfg->atcai2c.baud);

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

    if (WireECC.endTransmission() != txlength) {
        return ATCA_TX_FAIL;
    }

    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_receive(ATCAIface iface,
                            uint8_t word_address,
                            uint8_t *rxdata,
                            uint16_t *rxlength) {

    *rxlength = Wire1.requestFrom(word_address, (size_t)(*rxlength));

    if (*rxlength == 0) {
        return ATCA_RX_FAIL;
    }

    int value;
    size_t i = 0;

    Serial5.print("Getting RX length: ");
    Serial5.println(*rxlength);

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

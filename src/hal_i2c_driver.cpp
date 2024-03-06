/**
 * @brief Compilation unit for implementation of HAL i2c interface declared.
 * We have defined in the atca_config.h that we use i2c as the interface
 * for communicating with the ECC. The file atca_hal.h declares these functions
 * for i2c communication, but we have to provide the implementation for them.
 * Thus we have to have these functions here.
 */

#include "cryptoauthlib/lib/cryptoauthlib.h"

#include <Arduino.h>
#include <Wire.h>

#define WIRE     Wire
#define WIRE_MUX 2

ATCA_STATUS hal_i2c_init(__attribute__((unused)) ATCAIface iface,
                         ATCAIfaceCfg* cfg) {
    WIRE.swap(WIRE_MUX);
    WIRE.setClock(cfg->atcai2c.baud);
    WIRE.begin();

    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_post_init(__attribute__((unused)) ATCAIface iface) {
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_send(__attribute__((unused)) ATCAIface iface,
                         uint8_t word_address,
                         uint8_t* txdata,
                         int txlength) {

    WIRE.beginTransmission(word_address);

    // Custom implementation of writing n bytes since the one provided from the
    // Wire library doesn't take failing to send a single byte into
    // consideration
    size_t index = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
    while (index < txlength) {
        if (WIRE.write(txdata[index])) {
            index++;
        }
    }
#pragma GCC diagnostic pop

    WIRE.endTransmission();

    // The Wire interface blocks and checks the TWIx.MSTATUS flag for WIF, which
    // give us the indication that the transmit was completed, so we return
    // success here

    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_receive(__attribute__((unused)) ATCAIface iface,
                            uint8_t word_address,
                            uint8_t* rxdata,
                            uint16_t* rxlength) {

    *rxlength = WIRE.requestFrom(word_address, (size_t)(*rxlength));

    int value;
    size_t i = 0;

    while (i < *rxlength) {
        value = WIRE.read();

        if (value != -1) {
            rxdata[i] = (uint8_t)value;
            i++;
        }
    }

    return ATCA_SUCCESS;
}

ATCA_STATUS
hal_i2c_control(__attribute__((unused)) ATCAIface iface,
                __attribute__((unused)) uint8_t option,
                __attribute__((unused)) void* param,
                __attribute__((unused)) size_t paramlen) {
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS hal_i2c_release(__attribute__((unused)) void* hal_data) {
    WIRE.end();
    return ATCA_SUCCESS;
}

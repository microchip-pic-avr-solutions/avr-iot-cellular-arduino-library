/**
 * @brief Compilation unit for implementation of HAL i2c interface declared.
 * We have defined in the atca_config.h that we use i2c as the interface
 * for communicating with the ECC. The file atca_hal.h declares these functions
 * for i2c communication, but we have to provide the implementation for them.
 * Thus we have to have these functions here.
 */
#include "cryptoauthlib.h"

#include <Arduino.h>
#include <Wire.h>

#ifdef __AVR_AVR128DB48__ // MINI

#define WIRE     Wire
#define WIRE_MUX 2

#else
#ifdef __AVR_AVR128DB64__ // Non-Mini

#define WIRE     Wire1
#define WIRE_MUX 2

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

ATCA_STATUS hal_i2c_init(ATCAIface iface, ATCAIfaceCfg *cfg) {
    WIRE.swap(WIRE_MUX);
    WIRE.setClock(cfg->atcai2c.baud);
    WIRE.begin();

    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_post_init(ATCAIface iface) { return ATCA_SUCCESS; }

ATCA_STATUS hal_i2c_send(ATCAIface iface,
                         uint8_t word_address,
                         uint8_t *txdata,
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

ATCA_STATUS hal_i2c_receive(ATCAIface iface,
                            uint8_t word_address,
                            uint8_t *rxdata,
                            uint16_t *rxlength) {

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
hal_i2c_control(ATCAIface iface, uint8_t option, void *param, size_t paramlen) {
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS hal_i2c_release(void *hal_data) {
    WIRE.end();
    return ATCA_SUCCESS;
}

/**
 * @brief Compilation unit for implementation of HAL i2c interface declared.
 * We have defined in the atca_config.h that we use i2c as the interface
 * for communicating with the ECC. The file atca_hal.h declares these functions
 * for i2c communication, but we have to provide the implementation for them.
 * Thus we have to have these functions here.
 */
#include "cryptoauthlib.h"

ATCA_STATUS hal_i2c_init(ATCAIface iface, ATCAIfaceCfg *cfg) {

    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS hal_i2c_post_init(ATCAIface iface) { return ATCA_SUCCESS; }

ATCA_STATUS hal_i2c_send(ATCAIface iface,
                         uint8_t word_address,
                         uint8_t *txdata,
                         int txlength) {
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS hal_i2c_receive(ATCAIface iface,
                            uint8_t word_address,
                            uint8_t *rxdata,
                            uint16_t *rxlength) {
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS
hal_i2c_control(ATCAIface iface, uint8_t option, void *param, size_t paramlen) {
    return ATCA_UNIMPLEMENTED;
}

ATCA_STATUS hal_i2c_release(void *hal_data) { return ATCA_UNIMPLEMENTED; }

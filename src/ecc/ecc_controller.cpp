#include "ecc_controller.h"

#include "atca_cfgs.h"
#include "cryptoauthlib.h"

#include <Arduino.h>
#include <UART.h>

void eccControllerInitialize(uint8_t *random_number) {
    ATCAIfaceCfg cfg_atecc608b_i2c = {ATCA_I2C_IFACE,
                                      ATECC608B,
                                      {
                                          0xC0,  // Address of ECC
                                          1,     // Bus number
                                          400000 // Baud
                                      },
                                      800,
                                      20,
                                      NULL};

    ATCA_STATUS result = atcab_init(&cfg_atecc608b_i2c);

    if (result != ATCA_SUCCESS) {

        Serial5.print("Error whilst initializing cryptolib: ");
        Serial5.println(result);
    }

    result = atcab_random(random_number);

    if (result != ATCA_SUCCESS) {

        Serial5.print("Error whilst getting random number: ");
        Serial5.println(result);
    }
}

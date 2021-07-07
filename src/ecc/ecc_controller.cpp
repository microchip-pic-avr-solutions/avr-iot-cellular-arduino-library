#include "ecc_controller.h"

#include <Arduino.h>
#include <UART.h>
#include <cryptoauthlib.h>

bool eccControllerInitialize(void) {
    // Config for the ECC. This needs to be static since cryptolib defines a
    // pointer to it during the initialization process and stores that for
    // further operations so we don't want to store it on the stack.
    static ATCAIfaceCfg cfg_atecc608b_i2c = {ATCA_I2C_IFACE,
                                             ATECC608B,
                                             {
                                                 0x58,  // 7 bit address of ECC
                                                 2,     // Bus number
                                                 100000 // Baud
                                             },
                                             1560,
                                             20,
                                             NULL};

    ATCA_STATUS result = atcab_init(&cfg_atecc608b_i2c);

    if (result != ATCA_SUCCESS) {
#ifdef DEBUG
        Serial5.print("Error whilst initializing cryptolib: ");
        Serial5.println(result, HEX);
#endif
        return false;
    }

    return true;
}

bool eccControllerRetrieveSerialNumber(uint8_t *serial_number) {

    uint8_t result = atcab_read_serial_number(serial_number);
    if (result != ATCA_SUCCESS) {
#ifdef DEBUG
        Serial5.print("Error whilst retrieving serial number: ");
        Serial5.println(result, HEX);
#endif
        return false;
    }

    return true;
}

bool eccControllerGenerateKeyPair(const uint8_t key_id, uint8_t *public_key) {
    uint8_t result = atcab_genkey(key_id, public_key);
    if (result != ATCA_SUCCESS) {
#ifdef DEBUG
        Serial5.print("Error whilst generating key pair: ");
        Serial5.println(result, HEX);
#endif
        return false;
    }

    return true;
}

bool eccControllerRetrievePublicKey(const uint8_t key_id, uint8_t *public_key) {
    uint8_t result = atcab_get_pubkey(key_id, public_key);
    if (result != ATCA_SUCCESS) {
#ifdef DEBUG
        Serial5.print("Error whilst retrieving public key: ");
        Serial5.println(result, HEX);
#endif
        return false;
    }

    return true;
}

bool eccControllerSignMessage(const uint8_t key_id,
                              const uint8_t *message,
                              uint8_t *signature) {

    uint8_t result = atcab_sign(key_id, message, signature);
    if (result != ATCA_SUCCESS) {
#ifdef DEBUG
        Serial5.print("Error whilst signing: ");
        Serial5.println(result, HEX);
#endif
        return false;
    }

    return true;
}

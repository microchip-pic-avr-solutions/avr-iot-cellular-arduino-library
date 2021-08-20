#include <Arduino.h>
#include <atca_helpers.h>
#include <atcacert/atcacert_client.h>
#include <cryptoauthlib.h>

#include "cert_def_1_signer.h"
#include "cert_def_3_device.h"

void printCertificate(uint8_t *certificate, uint16_t size) {
    char buffer[1024];
    size_t buffer_size = sizeof(buffer);
    ATCA_STATUS result =
        atcab_base64encode(certificate, size, buffer, &buffer_size);

    if (result != ATCA_SUCCESS) {
        Serial5.printf("Failed to encode into base64: %x\r\n", result);
        return;
    }

    buffer[buffer_size] = 0;
    Serial5.printf(
        "-----BEGIN CERTIFICATE-----\r\n%s\r\n-----END CERTIFICATE-----\r\n",
        buffer);
}

void setup() {

    Serial5.begin(115200);

    ATCA_STATUS status;

    static ATCAIfaceCfg cfg_atecc608b_i2c = {ATCA_I2C_IFACE,
                                             ATECC608B,
                                             {
                                                 0x58,  // 7 bit address of ECC
                                                 2,     // Bus number
                                                 100000 // Baud rate
                                             },
                                             1560,
                                             20,
                                             NULL};

    if (ATCA_SUCCESS != (status = atcab_init(&cfg_atecc608b_i2c))) {
        Serial5.printf("Failed to init: %d\r\n", status);
        return;
    } else {
        Serial5.printf("Initialized ECC\r\n");
    }

    // Retrieve public root key
    uint8_t public_key[ATCA_PUB_KEY_SIZE];
    if (ATCA_SUCCESS != (status = atcab_get_pubkey(0, public_key))) {
        Serial5.printf("Failed to get public key: %x\r\n", status);
        return;
    }

    Serial5.println("\r\n\r\n");

    // Retrive sign certificate
    uint8_t buffer[g_cert_def_1_signer.cert_template_size + 4];
    size_t size = sizeof(buffer);

    if (ATCA_SUCCESS !=
        (status = atcacert_read_cert(
             &g_cert_def_1_signer, public_key, buffer, &size))) {
        Serial5.printf("Failed to read signing certificate: %d\r\n", status);
        return;
    } else {
        Serial5.println("Printing signing certificate...");
        printCertificate(buffer, size);
    }

    Serial5.println("\r\n\r\n");

    // Retrive device certificate
    if (ATCA_SUCCESS !=
        (status = atcacert_read_cert(
             &g_cert_def_3_device, public_key, buffer, &size))) {
        Serial5.printf("Failed to read device certificate: %d\r\n", status);
        return;
    } else {
        Serial5.println("Printing device certificate...");
        printCertificate(buffer, size);
    }
}

void loop() {}

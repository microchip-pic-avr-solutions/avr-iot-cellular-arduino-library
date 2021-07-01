#include "ecc_controller.h"

#include "cryptoauthlib.h"

void eccControllerInitialize(uint8_t *random_number) {
    atcab_init(NULL);
    atcab_random(&random_number);
}

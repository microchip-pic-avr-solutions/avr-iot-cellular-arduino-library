#ifndef ECC_CONTROLLER_H
#define ECC_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void eccControllerInitialize(uint8_t *random_number);

#ifdef __cplusplus
}
#endif

#endif

/* Auto-generated config file atca_config.h */
#ifndef ATCA_CONFIG_H
#define ATCA_CONFIG_H

/* Included HALS */
#define ATCA_HAL_I2C

#define ATCA_ATECC608_SUPPORT

/* \brief How long to wait after an initial wake failure for the POST to
 *         complete.
 * If Power-on self test (POST) is enabled, the self test will run on waking
 * from sleep or during power-on, which delays the wake reply.
 */
#ifndef ATCA_POST_DELAY_MSEC
#define ATCA_POST_DELAY_MSEC 25
#endif

/***************** Diagnostic & Test Configuration Section *****************/

/** Enable debug messages */
// #define ATCA_PRINTF
//

/******************** Platform Configuration Section ***********************/

/** Define platform malloc/free */
#define ATCA_PLATFORM_MALLOC malloc
#define ATCA_PLATFORM_FREE   free

#define __DELAY_BACKWARD_COMPATIBLE__

#include <util/delay.h>

#define atca_delay_ms _delay_ms
#define atca_delay_us _delay_us

#endif // ATCA_CONFIG_H

#ifndef ATCA_CONFIG_H
#define ATCA_CONFIG_H

#define __DELAY_BACKWARD_COMPATIBLE__

#include <util/delay.h>

#define atca_delay_ms _delay_ms
#define atca_delay_us _delay_us

/* Included HALS */
#define ATCA_HAL_I2C

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

/******************** Platform Configuration Section ***********************/

#define ATCA_ATECC608_SUPPORT FEATURE_ENABLED
#define ATCA_TFLEX_SUPPORT    FEATURE_ENABLED

/** Define platform malloc/free */
#define ATCA_PLATFORM_MALLOC malloc
#define ATCA_PLATFORM_FREE   free

/* API Configuration Options */
#define ATCAB_AES_EN                      FEATURE_DISABLED
#define ATCAB_AES_GCM_EN                  FEATURE_DISABLED
#define ATCAB_COUNTER_EN                  FEATURE_DISABLED
#define ATCAB_DERIVEKEY_EN                FEATURE_DISABLED
#define ATCAB_ECDH_EN                     FEATURE_DISABLED
#define ATCAB_ECDH_ENC_EN                 FEATURE_DISABLED
#define ATCAB_GENDIG_EN                   FEATURE_DISABLED
#define ATCAB_GENKEY_MAC_EN               FEATURE_DISABLED
#define ATCAB_HMAC_EN                     FEATURE_DISABLED
#define ATCAB_INFO_LATCH_EN               FEATURE_DISABLED
#define ATCAB_KDF_EN                      FEATURE_DISABLED
#define ATCAB_LOCK_EN                     FEATURE_DISABLED
#define ATCAB_MAC_EN                      FEATURE_DISABLED
#define ATCAB_PRIVWRITE_EN                FEATURE_DISABLED
#define ATCAB_AES_GCM_EN                  FEATURE_DISABLED
#define ATCAB_AES_CCM_EN                  FEATURE_DISABLED
#define ATCAB_AES_CBC_ENCRYPT_EN          FEATURE_DISABLED
#define ATCAB_RANDOM_EN                   FEATURE_DISABLED
#define ATCAB_READ_ENC_EN                 FEATURE_DISABLED
#define ATCAB_WRITE_ENC_EN                FEATURE_DISABLED
#define ATCAB_SECUREBOOT_EN               FEATURE_DISABLED
#define ATCAB_SECUREBOOT_MAC_EN           FEATURE_DISABLED
#define ATCAB_SELFTEST_EN                 FEATURE_DISABLED
#define ATCAB_SHA_HMAC_EN                 FEATURE_DISABLED
#define ATCAB_SIGN_INTERNAL_EN            FEATURE_DISABLED
#define ATCAB_UPDATEEXTRA_EN              FEATURE_ENABLED
#define ATCAB_AES_CCM_RAND_IV_EN          FEATURE_DISABLED
#define ATCAB_VERIFY_EXTERN_STORED_MAC_EN FEATURE_DISABLED
#define ATCAB_AES_GFM_EN                  FEATURE_DISABLED
#define ATCAB_VERIFY_EN                   FEATURE_DISABLED
#define ATCAB_WRITE_EN                    FEATURE_ENABLED
#define ATCAC_SHA1_EN                     FEATURE_ENABLED
#define ATCAC_SHA256_EN                   FEATURE_ENABLED

#define TALIB_AES_EN      FEATURE_DISABLED
#define TALIB_SHA_HMAC_EN FEATURE_DISABLED

#define CALIB_SHA104_EN      FEATURE_DISABLED
#define CALIB_SHA105_EN      FEATURE_DISABLED
#define CALIB_SHA204_EN      FEATURE_DISABLED
#define CALIB_SHA206_EN      FEATURE_DISABLED
#define CALIB_ECC108_EN      FEATURE_DISABLED
#define CALIB_ECC204_EN      FEATURE_DISABLED
#define CALIB_ECC508_EN      FEATURE_DISABLED
#define CALIB_TA010_EN       FEATURE_DISABLED
#define CALIB_ECDH_ENC       FEATURE_DISABLED
#define CALIB_WRITE_EN       FEATURE_ENABLED
#define CALIB_UPDATEEXTRA_EN FEATURE_ENABLED

#define CALIB_WRITE_ENC_CA2_EN    FEATURE_DISABLED
#define CALIB_WRITE_ENC_ECC204_EN FEATURE_DISABLED

#define WPC_MSG_PR_EN FEATURE_DISABLED

#define ATCACERT_DATEFMT_UTC_EN FEATURE_ENABLED
#define ATCACERT_DATEFMT_GEN_EN FEATURE_ENABLED

#define ATCACERT_DATEFMT_ISO_EN   FEATURE_DISABLED
#define ATCACERT_DATEFMT_POSIX_EN FEATURE_DISABLED

#endif // ATCA_CONFIG_H
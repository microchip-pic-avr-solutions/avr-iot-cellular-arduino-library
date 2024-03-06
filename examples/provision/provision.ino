/**
 * @brief This provisioning is for HTTPS as well as MQTT with TLS (with or
 * without the ECC). In order to start the provisioning, just upload this sketch
 * to the board.
 */

#include <avr/pgmspace.h>

#include "ecc608.h"
#include "led_ctrl.h"
#include "log.h"
#include "security_profile.h"
#include "sequans_controller.h"

#include "cryptoauthlib/lib/atcacert/atcacert_client.h"
#include "cryptoauthlib/lib/atcacert/atcacert_def.h"
#include "cryptoauthlib/lib/cryptoauthlib.h"

#include <stdint.h>
#include <string.h>

enum class Service : uint8_t { AzureIoTHub = 1, AWS, Other };

#define MQTT_TLS                         (1)
#define MQTT_TLS_PUBLIC_PRIVATE_KEY_PAIR (2)

#define DEFAULT_CA_SLOT       (1)
#define HTTP_CUSTOM_CA_SLOT   (15)
#define MQTT_CUSTOM_CA_SLOT   (16)
#define MQTT_PUBLIC_KEY_SLOT  (17)
#define MQTT_PRIVATE_KEY_SLOT (17)

static const char AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC[] PROGMEM =
    "AT+SQNSPCFG=1,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\",1";
static const char AT_MQTT_SECURITY_PROFILE[] PROGMEM =
    "AT+SQNSPCFG=2,%u,\"%s\",%u,%u,,,\"%s\",\"%s\"";
static const char AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES[] PROGMEM =
    "AT+SQNSPCFG=2,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\"";

static const char AT_HTTPS_SECURITY_PROFILE[] PROGMEM =
    "AT+SQNSPCFG=3,%u,\"\",%u,%u";

static const char AT_WRITE_CERTIFICATE[] PROGMEM =
    "AT+SQNSNVW=\"certificate\",%u,%u";
static const char AT_ERASE_CERTIFICATE[] PROGMEM =
    "AT+SQNSNVW=\"certificate\",%u,0";
static const char AT_WRITE_PRIVATE_KEY[] PROGMEM =
    "AT+SQNSNVW=\"privatekey\",%u,%u";
static const char AT_ERASE_PRIVATE_KEY[] PROGMEM =
    "AT+SQNSNVW=\"privatekey\",%u,0";

#define NUMBER_OF_CIPHERS      (64)
#define CIPHER_VALUE_LENGTH    (6)
#define CIPHER_TEXT_MAX_LENGTH (50)

#define ASCII_CARRIAGE_RETURN (0xD)
#define ASCII_LINE_FEED       (0xA)
#define ASCII_BACKSPACE       (0x8)
#define ASCII_DELETE          (0x7F)
#define ASCII_SPACE           (0x20)

#define SerialModule Serial3

#define INFO_SECTION_OFFSET (13)

#define CERTIFICATE_REQUEST_LENGTH_OFFSET (2)
#define DATA_LENGTH_OFFSET                (6)
#define INFO_SECTION_LENGTH_OFFSET        (12)

#define INFO_ENTRY_SET_LENGTH_OFFSET      (1)
#define INFO_ENTRY_SEQUENCE_LENGTH_OFFSET (3)
#define INFO_ENTRY_DATA_LENGTH_OFFSET     (10)

// clang-format off
const char cipher0[7] PROGMEM = "0x1301"; // TLS_AES_128_GCM_SHA256
const char cipher1[7] PROGMEM = "0x1302"; // TLS_AES_256_GCM_SHA384
const char cipher2[7] PROGMEM = "0x1303"; // TLS_CHACHA20_POLY1305_SHA256
const char cipher3[7] PROGMEM = "0x1304"; // TLS_AES_128_CCM_SHA256
const char cipher4[7] PROGMEM = "0x1305"; // TLS_AES_128_CCM_8_SHA256
const char cipher5[7] PROGMEM = "0x000A"; // SSL_RSA_WITH_3DES_EDE_CBC_SHA
const char cipher6[7] PROGMEM = "0x002F"; // TLS_RSA_WITH_AES_128_CBC_SHA
const char cipher7[7] PROGMEM = "0x0035"; // TLS_RSA_WITH_AES_256_CBC_SHA
const char cipher8[7] PROGMEM = "0x0033"; // TLS_DHE_RSA_WITH_AES_128_CBC_SHA
const char cipher9[7] PROGMEM = "0x0039"; // TLS_DHE_RSA_WITH_AES_256_CBC_SHA
const char cipher10[7] PROGMEM = "0x00AB"; // TLS_DHE_PSK_WITH_AES_256_GCM_SHA384
const char cipher11[7] PROGMEM = "0x00AA"; // TLS_DHE_PSK_WITH_AES_128_GCM_SHA256
const char cipher12[7] PROGMEM = "0x00A9"; // TLS_PSK_WITH_AES_256_GCM_SHA384
const char cipher13[7] PROGMEM = "0x00A8"; // TLS_PSK_WITH_AES_128_GCM_SHA256
const char cipher14[7] PROGMEM = "0x00B3"; // TLS_DHE_PSK_WITH_AES_256_CBC_SHA384
const char cipher15[7] PROGMEM = "0x00B2"; // TLS_DHE_PSK_WITH_AES_128_CBC_SHA256
const char cipher16[7] PROGMEM = "0x00AF"; // TLS_PSK_WITH_AES_256_CBC_SHA384
const char cipher17[7] PROGMEM = "0x00AE"; // TLS_PSK_WITH_AES_128_CBC_SHA256
const char cipher18[7] PROGMEM = "0x008C"; // TLS_PSK_WITH_AES_128_CBC_SHA
const char cipher19[7] PROGMEM = "0x008D"; // TLS_PSK_WITH_AES_256_CBC_SHA
const char cipher20[7] PROGMEM = "0xC0A6"; // TLS_DHE_PSK_WITH_AES_128_CCM
const char cipher21[7] PROGMEM = "0xC0A7"; // TLS_DHE_PSK_WITH_AES_256_CCM
const char cipher22[7] PROGMEM = "0xC0A4"; // TLS_PSK_WITH_AES_128_CCM
const char cipher23[7] PROGMEM = "0xC0A5"; // TLS_PSK_WITH_AES_256_CCM
const char cipher24[7] PROGMEM = "0xC0A8"; // TLS_PSK_WITH_AES_128_CCM_8
const char cipher25[7] PROGMEM = "0xC0A9"; // TLS_PSK_WITH_AES_256_CCM_8
const char cipher26[7] PROGMEM = "0xC0A0"; // TLS_RSA_WITH_AES_128_CCM_8
const char cipher27[7] PROGMEM = "0xC0A1"; // TLS_RSA_WITH_AES_256_CCM_8
const char cipher28[7] PROGMEM = "0xC0AC"; // TLS_ECDHE_ECDSA_WITH_AES_128_CCM
const char cipher29[7] PROGMEM = "0xC0AE"; // TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8
const char cipher30[7] PROGMEM = "0xC0AF"; // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
const char cipher31[7] PROGMEM = "0xC014"; // TLS_EDHE_ECDSA_WITH_AES_256_CCM_8
const char cipher32[7] PROGMEM = "0xC013"; // TLS_ECCDHE_RSA_WITH_AES_256_CBC_SHA
const char cipher33[7] PROGMEM = "0xC009"; // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
const char cipher34[7] PROGMEM = "0xC00A"; // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
const char cipher35[7] PROGMEM = "0xC012"; // TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
const char cipher36[7] PROGMEM = "0xC008"; // TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
const char cipher37[7] PROGMEM = "0x003C"; // TLS_RSA_WITH_AES_128_CBC_SHA256
const char cipher38[7] PROGMEM = "0x003D"; // TLS_RSA_WITH_AES_256_CBC_SHA256
const char cipher39[7] PROGMEM = "0x0067"; // TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
const char cipher40[7] PROGMEM = "0x006B"; // TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
const char cipher41[7] PROGMEM = "0x009C"; // TLS_RSA_WITH_AES_128_GCM_SHA256
const char cipher42[7] PROGMEM = "0x009D"; // TLS_RSA_WITH_AES_256_GCM_SHA384
const char cipher43[7] PROGMEM = "0x009E"; // TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
const char cipher44[7] PROGMEM = "0x009F"; // TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
const char cipher45[7] PROGMEM = "0xC02F"; // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
const char cipher46[7] PROGMEM = "0xC030"; // TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
const char cipher47[7] PROGMEM = "0xC02B"; // TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
const char cipher48[7] PROGMEM = "0xC02C"; // TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
const char cipher49[7] PROGMEM = "0xC027"; // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
const char cipher50[7] PROGMEM = "0xC023"; // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
const char cipher51[7] PROGMEM = "0xC028"; // TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
const char cipher52[7] PROGMEM = "0xC024"; // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
const char cipher53[7] PROGMEM = "0xCCA8"; // TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
const char cipher54[7] PROGMEM = "0xCCA9"; // TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
const char cipher55[7] PROGMEM = "0xCCAA"; // TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256
const char cipher56[7] PROGMEM = "0xCC13"; // TLS_ECDHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256
const char cipher57[7] PROGMEM = "0xCC14"; // TLS_ECDHE_ECDSA_WITH_CHACHA20_OLD_POLY1305_SHA256
const char cipher58[7] PROGMEM = "0xCC15"; // TLS_DHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256
const char cipher59[7] PROGMEM = "0xC037"; // TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
const char cipher60[7] PROGMEM = "0xCCAB"; // TLS_PSK_WITH_CHACHA20_POLY1305_SHA256
const char cipher61[7] PROGMEM = "0xCCAC"; // TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256
const char cipher62[7] PROGMEM = "0xCCAD"; // TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256
const char cipher63[7] PROGMEM = "0x0016"; // TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA

// clang-format on

PGM_P const cipher_values[NUMBER_OF_CIPHERS] PROGMEM = {
    cipher0,  cipher1,  cipher2,  cipher3,  cipher4,  cipher5,  cipher6,
    cipher7,  cipher8,  cipher9,  cipher10, cipher11, cipher12, cipher13,
    cipher14, cipher15, cipher16, cipher17, cipher18, cipher19, cipher20,
    cipher21, cipher22, cipher23, cipher24, cipher25, cipher26, cipher27,
    cipher28, cipher29, cipher30, cipher31, cipher32, cipher33, cipher34,
    cipher35, cipher36, cipher37, cipher38, cipher39, cipher40, cipher41,
    cipher42, cipher43, cipher44, cipher45, cipher46, cipher47, cipher48,
    cipher49, cipher50, cipher51, cipher52, cipher53, cipher54, cipher55,
    cipher56, cipher57, cipher58, cipher59, cipher60, cipher61, cipher62,
    cipher63};

// clang-format off
const char cipher_text_0[50] PROGMEM = "TLS_AES_128_GCM_SHA256";
const char cipher_text_1[50] PROGMEM = "TLS_AES_256_GCM_SHA384";
const char cipher_text_2[50] PROGMEM = "TLS_CHACHA20_POLY1305_SHA256";
const char cipher_text_3[50] PROGMEM = "TLS_AES_128_CCM_SHA256";
const char cipher_text_4[50] PROGMEM = "TLS_AES_128_CCM_8_SHA256";
const char cipher_text_5[50] PROGMEM = "SSL_RSA_WITH_3DES_EDE_CBC_SHA";
const char cipher_text_6[50] PROGMEM = "TLS_RSA_WITH_AES_128_CBC_SHA";
const char cipher_text_7[50] PROGMEM = "TLS_RSA_WITH_AES_256_CBC_SHA";
const char cipher_text_8[50] PROGMEM = "TLS_DHE_RSA_WITH_AES_128_CBC_SHA";
const char cipher_text_9[50] PROGMEM = "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
const char cipher_text_10[50] PROGMEM = "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384";
const char cipher_text_11[50] PROGMEM = "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256";
const char cipher_text_12[50] PROGMEM = "TLS_PSK_WITH_AES_256_GCM_SHA384";
const char cipher_text_13[50] PROGMEM = "TLS_PSK_WITH_AES_128_GCM_SHA256";
const char cipher_text_14[50] PROGMEM = "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384";
const char cipher_text_15[50] PROGMEM = "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256";
const char cipher_text_16[50] PROGMEM = "TLS_PSK_WITH_AES_256_CBC_SHA384";
const char cipher_text_17[50] PROGMEM = "TLS_PSK_WITH_AES_128_CBC_SHA256";
const char cipher_text_18[50] PROGMEM = "TLS_PSK_WITH_AES_128_CBC_SHA";
const char cipher_text_19[50] PROGMEM = "TLS_PSK_WITH_AES_256_CBC_SHA";
const char cipher_text_20[50] PROGMEM = "TLS_DHE_PSK_WITH_AES_128_CCM";
const char cipher_text_21[50] PROGMEM = "TLS_DHE_PSK_WITH_AES_256_CCM";
const char cipher_text_22[50] PROGMEM = "TLS_PSK_WITH_AES_128_CCM";
const char cipher_text_23[50] PROGMEM = "TLS_PSK_WITH_AES_256_CCM";
const char cipher_text_24[50] PROGMEM = "TLS_PSK_WITH_AES_128_CCM_8";
const char cipher_text_25[50] PROGMEM = "TLS_PSK_WITH_AES_256_CCM_8";
const char cipher_text_26[50] PROGMEM = "TLS_RSA_WITH_AES_128_CCM_8";
const char cipher_text_27[50] PROGMEM = "TLS_RSA_WITH_AES_256_CCM_8";
const char cipher_text_28[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_128_CCM";
const char cipher_text_29[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8";
const char cipher_text_30[50] PROGMEM = "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA";
const char cipher_text_31[50] PROGMEM = "TLS_EDHE_ECDSA_WITH_AES_256_CCM_8";
const char cipher_text_32[50] PROGMEM = "TLS_ECCDHE_RSA_WITH_AES_256_CBC_SHA";
const char cipher_text_33[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA";
const char cipher_text_34[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA";
const char cipher_text_35[50] PROGMEM = "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA";
const char cipher_text_36[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA";
const char cipher_text_37[50] PROGMEM = "TLS_RSA_WITH_AES_128_CBC_SHA256";
const char cipher_text_38[50] PROGMEM = "TLS_RSA_WITH_AES_256_CBC_SHA256";
const char cipher_text_39[50] PROGMEM = "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256";
const char cipher_text_40[50] PROGMEM = "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256";
const char cipher_text_41[50] PROGMEM = "TLS_RSA_WITH_AES_128_GCM_SHA256";
const char cipher_text_42[50] PROGMEM = "TLS_RSA_WITH_AES_256_GCM_SHA384";
const char cipher_text_43[50] PROGMEM = "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256";
const char cipher_text_44[50] PROGMEM = "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384";
const char cipher_text_45[50] PROGMEM = "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
const char cipher_text_46[50] PROGMEM = "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
const char cipher_text_47[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
const char cipher_text_48[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
const char cipher_text_49[50] PROGMEM = "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256";
const char cipher_text_50[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256";
const char cipher_text_51[50] PROGMEM = "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384";
const char cipher_text_52[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384";
const char cipher_text_53[50] PROGMEM = "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256";
const char cipher_text_54[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256";
const char cipher_text_55[50] PROGMEM = "TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256";
const char cipher_text_56[50] PROGMEM = "TLS_ECDHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256";
const char cipher_text_57[50] PROGMEM = "TLS_ECDHE_ECDSA_WITH_CHACHA20_OLD_POLY1305_SHA256";
const char cipher_text_58[50] PROGMEM = "TLS_DHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256";
const char cipher_text_59[50] PROGMEM = "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256";
const char cipher_text_60[50] PROGMEM = "TLS_PSK_WITH_CHACHA20_POLY1305_SHA256";
const char cipher_text_61[50] PROGMEM = "TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256";
const char cipher_text_62[50] PROGMEM = "TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256";
const char cipher_text_63[50] PROGMEM = "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
// clang-format on

PGM_P const cipher_text_values[NUMBER_OF_CIPHERS] PROGMEM = {
    cipher_text_0,  cipher_text_1,  cipher_text_2,  cipher_text_3,
    cipher_text_4,  cipher_text_5,  cipher_text_6,  cipher_text_7,
    cipher_text_8,  cipher_text_9,  cipher_text_10, cipher_text_11,
    cipher_text_12, cipher_text_13, cipher_text_14, cipher_text_15,
    cipher_text_16, cipher_text_17, cipher_text_18, cipher_text_19,
    cipher_text_20, cipher_text_21, cipher_text_22, cipher_text_23,
    cipher_text_24, cipher_text_25, cipher_text_26, cipher_text_27,
    cipher_text_28, cipher_text_29, cipher_text_30, cipher_text_31,
    cipher_text_32, cipher_text_33, cipher_text_34, cipher_text_35,
    cipher_text_36, cipher_text_37, cipher_text_38, cipher_text_39,
    cipher_text_40, cipher_text_41, cipher_text_42, cipher_text_43,
    cipher_text_44, cipher_text_45, cipher_text_46, cipher_text_47,
    cipher_text_48, cipher_text_49, cipher_text_50, cipher_text_51,
    cipher_text_52, cipher_text_53, cipher_text_54, cipher_text_55,
    cipher_text_56, cipher_text_57, cipher_text_58, cipher_text_59,
    cipher_text_60, cipher_text_61, cipher_text_62, cipher_text_63};

// The following are all ASN1 templates

// clang-format off
const uint8_t country_code_template[] = {
    0x31, 0x00,                                         // Start set
    0x30, 0x00,                                         // Start sequence
    0x06, 0x03, 0x55, 0x04, 0x06,                       // Object identifier
    0x13, 0x00                                          // Country code data
};

const uint8_t state_or_province_name_template[] = {
    0x31, 0x00,                                         // Start set
    0x30, 0x00,                                         // Start sequence
    0x06, 0x03, 0x55, 0x04, 0x08,                       // Object identifier
    0x0C, 0x00,                                         // Start of UTF8String
};


const uint8_t locality_name_template[] = {
    0x31, 0x00,                                         // Start set
    0x30, 0x00,                                         // Start sequence
    0x06, 0x03, 0x55, 0x04, 0x07,                       // Object identifier
    0x0C, 0x00,                                         // Start of UTF8String
};

const uint8_t organization_name_template[] = {
    0x31, 0x22,                                         // Start set
    0x30, 0x20,                                         // Start sequence
    0x06, 0x03, 0x55, 0x04, 0x0A,                       // Object identifier
    0x0C, 0x19, 0x4d, 0x69, 0x63, 0x72, 0x6f, 0x63,     // Organization name:
    0x68, 0x69, 0x70, 0x20, 0x54, 0x65, 0x63, 0x68,     // Microchip Technology
    0x6e, 0x6f, 0x6c, 0x6f, 0x67, 0x79, 0x20, 0x49,     // Inc.
    0x6e, 0x63, 0x2e
};

const uint8_t common_name_template[] = {
    0x31, 0x00,                                         // Start set
    0x30, 0x00,                                         // Start sequence
    0x06, 0x03, 0x55, 0x04, 0x03,                       // Object identifier
    0x0C, 0x00,                                         // Start of UTF8String
};
// clang-format on

const uint8_t version_template[] = {0x2, 0x1, 0x0};

const uint8_t ec_public_key_template[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01,
    0x07, 0x03, 0x42, 0x00, 0x04, 0xd8, 0x70, 0xa4, 0xdf, 0x98, 0xb4,
    0x6a, 0x93, 0x2b, 0xf7, 0x40, 0x39, 0x86, 0x0f, 0xed, 0xd6, 0x69,
    0x03, 0x6a, 0xe7, 0xe4, 0x84, 0x9f, 0xfc, 0xfb, 0x61, 0x50, 0x63,
    0x21, 0x95, 0xa8, 0x91, 0x2c, 0x98, 0x04, 0x0e, 0x9c, 0x2f, 0x03,
    0xe1, 0xe4, 0x2e, 0xc7, 0x93, 0x8c, 0x6b, 0xf4, 0xfb, 0x98, 0x4c,
    0x50, 0xdb, 0x51, 0xa3, 0xee, 0x04, 0x1b, 0x55, 0xf0, 0x60, 0x63,
    0xeb, 0x46, 0x90, 0xa0, 0x11, 0x30, 0x0f, 0x06, 0x09, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x0e, 0x31, 0x02, 0x30, 0x00};

const uint8_t signature_template[]{
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03,
    0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x20, 0x26, 0xab, 0x8a,
    0x4f, 0x71, 0x2c, 0xf9, 0xbb, 0x4f, 0xfa, 0xa4, 0xcd, 0x01, 0x48,
    0xf1, 0xdf, 0x9c, 0xdc, 0xff, 0xa0, 0xff, 0x53, 0x8f, 0x35, 0x8d,
    0xd4, 0x3d, 0x49, 0xc0, 0x72, 0xf5, 0x0a, 0x02, 0x21, 0x00, 0xa5,
    0x9d, 0xb4, 0x11, 0x4b, 0xa1, 0x65, 0x7c, 0xbb, 0x48, 0xcf, 0x6d,
    0xf6, 0xd0, 0x6a, 0x41, 0x00, 0x96, 0xe1, 0xe2, 0x79, 0x73, 0xdb,
    0xf7, 0x97, 0x80, 0x41, 0x9b, 0x35, 0x01, 0x88, 0x5e};

/**
 * @brief Will read until new line or carriage return. Will clear values from
 * the buffer if the user does a backspace or delete.
 *
 * @param output_buffer Content is placed in this output buffer.
 * @param output_buffer_length The length of the output buffer
 *
 * @return false if the amount of characters read is greater than
 */
static bool readUntilNewLine(char* output_buffer,
                             const size_t output_buffer_length) {

    size_t index = 0;

    while (true) {

        while (!SerialModule.available()) {}

        uint8_t input = SerialModule.read();

        if (input == ASCII_SPACE && index == 0) {
            continue;
        }

        if (input == ASCII_CARRIAGE_RETURN || input == ASCII_LINE_FEED) {
            break;
        } else if (input == ASCII_BACKSPACE || input == ASCII_DELETE) {
            if (index > 0) {
                // Prevent removing the printed text, only remove if the
                // user has inputted some text
                SerialModule.print((char)input);
                output_buffer[index] = '\0';
                index--;
            }

            continue;
        }

        if (output_buffer == nullptr || output_buffer_length == 0) {
            continue;
        }

        SerialModule.print((char)input);

        // - 1 here since we need space for null termination
        if (index >= output_buffer_length - 1) {
            return false;
        }

        output_buffer[index++] = input;

        // Fill null termination as we go
        output_buffer[index] = '\0';
    }

    return true;
}

/**
 * @brief Asks a yes/no question.
 *
 * @return true if yes, false if no.
 */
static bool askCloseEndedQuestion(const __FlashStringHelper* question) {
    while (true) {
        SerialModule.println(question);
        SerialModule.print(F("Please choose (y/n). Press enter when done: "));

        // We have a much larger input buffer here to support the giving
        // feedback for longer unsupported inputs by the user
        char input[32] = "";

        if (!readUntilNewLine(input, sizeof(input))) {
            SerialModule.println(F("\r\nInput too long!"));
            continue;
        }
        switch (input[0]) {
        case 'y':
            return true;

        case 'Y':
            return true;

        case 'n':
            return false;

        case 'N':
            return false;

        default:
            SerialModule.print(F("\r\nNot supported choice made: "));
            SerialModule.println(input);
            SerialModule.println();
            break;
        }
    }
}

/**
 * @brief Ask a question where a list of alternatives are given. Note that there
 * cannot be a hole in the values ranging from @p min_value from @p max_value.
 *
 * @param question The question to ask, this is specified as a string stored in
 * program memory.
 * @param min_value Minimum value for the choices presented.
 * @param max_value Maximum value for the choices presented.
 *
 * @return The choice picked.
 */
static uint8_t askNumberedQuestion(const __FlashStringHelper* question,
                                   const uint8_t min_value,
                                   const uint8_t max_value) {
    while (true) {
        SerialModule.print(question);

        // We have a much larger input buffer here to support the giving
        // feedback for longer unsupported inputs by the user
        char input[32] = "";

        if (!readUntilNewLine(input, sizeof(input))) {
            SerialModule.println(F("\r\nInput too long!\r\n"));
            continue;
        }

        const uint8_t input_number = strtol(input, NULL, 10);

        if (input_number >= min_value && input_number <= max_value) {
            return input_number;
        } else {
            SerialModule.print(F("\r\nNot supported choice made: "));
            SerialModule.println(input);
            SerialModule.println();
        }
    }
}

/**
 * @brief Ask a question to the user where the input is just text.
 *
 * @param question The question to ask.
 * @param output_buffer Where the input from the user is placed.
 * @param output_buffer_size The max size of the input - 1 for NULL termination.
 */
static void askInputQuestion(const __FlashStringHelper* question,
                             char* output_buffer,
                             const size_t output_buffer_size) {

    while (true) {
        SerialModule.print(question);

        if (!readUntilNewLine(output_buffer, output_buffer_size)) {
            SerialModule.println(F("\r\nInput too long!\r\n"));
            continue;
        } else {
            return;
        }
    }
}

/**
 * @brief Buffer for commands. Used as a re-usable buffer to save stack usage.
 */
static char command_buffer[256] = "";

/**
 * @brief Writes a certificate to the Sequans modem.
 *
 * @param slot [in] The slot to write the certificate to.
 * @param certificate_data [in] The certificate data.
 *
 * @return ResponseResult::OK if the certificate was written successfully.
 */
static ResponseResult writeCertificate(const uint8_t slot,
                                       const char* certificate_data) {

    SequansController.clearReceiveBuffer();

    // First erase the existing certificate at the slot (if any)
    snprintf_P(command_buffer,
               sizeof(command_buffer),
               AT_ERASE_CERTIFICATE,
               slot);

    SequansController.writeBytes((uint8_t*)command_buffer,
                                 strlen(command_buffer),
                                 true);

    // Dummy read of response, we don't care about the response as the modem
    // will return an error if there's no certificate to erase and OK if it
    // was ereased
    SequansController.readResponse();

    const size_t certificate_length = strlen(certificate_data);

    snprintf_P(command_buffer,
               sizeof(command_buffer),
               AT_WRITE_CERTIFICATE,
               slot,
               certificate_length);

    SequansController.writeBytes((uint8_t*)command_buffer,
                                 strlen(command_buffer),
                                 true);

    if (SequansController.waitForByte('>', 1000)) {
        SequansController.writeBytes((uint8_t*)certificate_data,
                                     certificate_length,
                                     true);
    }

    return SequansController.readResponse();
}

/**
 * @brief Writes a private key to the Sequans modem.
 *
 * @param slot [in] The slot to write the private key to.
 * @param private_key_data  [in] The private key data.
 *
 * @return ResponseResult::OK if the private key was written successfully.
 */
static ResponseResult writePrivateKey(const uint8_t slot,
                                      const char* private_key_data) {
    SequansController.clearReceiveBuffer();

    // First erase the existing certificate at the slot (if any)
    snprintf_P(command_buffer,
               sizeof(command_buffer),
               AT_ERASE_PRIVATE_KEY,
               slot);

    SequansController.writeBytes((uint8_t*)command_buffer,
                                 strlen(command_buffer),
                                 true);

    // Dummy read of response, we don't care about the response as the modem
    // will return an error if there's no certificate to erase and OK if it
    // was ereased
    SequansController.readResponse();

    const size_t private_key_length = strlen(private_key_data);

    snprintf_P(command_buffer,
               sizeof(command_buffer),
               AT_WRITE_PRIVATE_KEY,
               slot,
               private_key_length);

    SequansController.writeBytes((uint8_t*)command_buffer,
                                 strlen(command_buffer),
                                 true);

    if (SequansController.waitForByte('>', 1000)) {
        SequansController.writeBytes((uint8_t*)private_key_data,
                                     private_key_length,
                                     true);
    }

    return SequansController.readResponse();
}

/**
 * @brief Buffer for certificate data. Used as a re-usable buffer to save stack
 * usage.
 */
static char data_buffer[6144] = "";

/**
 * @brief Asks the user to input a certificate/private key and saves that
 * the NVM for the Sequans modem at the given @p slot.
 *
 * @param message Inital message presented to the user.
 * @param slot Slot to save the ceritficate/private key in.
 * @param is_certificate Whether to save to certificate or private key
 * space.
 *
 * @return true If write was successful.
 */
static bool requestAndSaveToNVM(const __FlashStringHelper* message,
                                const uint8_t slot,
                                const bool is_certificate) {

    bool got_data = false;
    size_t index  = 0;

    while (!got_data) {

        memset(data_buffer, '\0', sizeof(data_buffer));
        SerialModule.println(message);

        index = 0;

        data_buffer[index++] = '\n';

        // Parse line by line until the end is found
        while (true) {

            // A line in x509 is 64 bytes + 1 for null termination
            char line[65] = "";

            if (!readUntilNewLine(line, sizeof(line))) {
                SerialModule.println(F("\r\nLine input too long!"));
                return false;
            }

            SerialModule.println();

            const size_t line_length = strlen(line);

            // If the user pressed enter, we are done
            if (line_length == 0) {
                got_data = true;
                break;
            }

            // +1 here for carriage return
            if (index + line_length + 1 >= sizeof(data_buffer)) {

                SerialModule.print(
                    is_certificate
                        ? F("\r\nCertificate longer than allowed size of ")
                        : F("\r\nPrivate key longer than allowed size of "));

                SerialModule.print(sizeof(data_buffer));
                SerialModule.println(F(" bytes.\r\n"));

                // Flush out the rest of the input. We do this timer based since
                // just checking available won't catch if there is a light delay
                // between each character processed
                uint32_t timer = millis();
                while (millis() - timer < 1000) {
                    if (SerialModule.available()) {
                        timer = millis();
                        SerialModule.read();
                    }
                }

                return false;
            }

            memcpy(&data_buffer[index], line, line_length);
            index += line_length;

            // Append carriage return to every line except the last
            data_buffer[index++] = '\n';
        }
    }

    data_buffer[index] = '\0';

    SerialModule.print(is_certificate ? F("Writing certificate... ")
                                      : F("Writing private key... "));

    ResponseResult result = ResponseResult::OK;

    if (is_certificate) {
        result = writeCertificate(slot, data_buffer);
    } else {
        result = writePrivateKey(slot, data_buffer);
    }

    if (result != ResponseResult::OK) {
        SerialModule.print(
            is_certificate ? F("Error occurred whilst storing certificate.")
                           : F("Error occurred whilst storing private key."));
        SerialModule.printf(F(" Error code: %X\r\n"),
                            static_cast<uint8_t>(result));
        return false;
    } else {
        SerialModule.println(F("Done"));
    }

    return true;
}

/**
 * @brief Writes the ECC's device certificate to the modem. This function also
 * calculates the SHA256 thumbprint of the certificate and optionally prints the
 * device certificate to the serial port.
 *
 * @note We pack some extra functionality in this function (SHA256 sum and
 * printing of the device certificate) to avoid having to do two separate reads
 * of the device certificate and save stack usage.
 *
 * @param slot [in] The slot to write to.
 * @param output_certificate [in] Whether we should output the certificate to
 * the serial port.
 * @param out_sha256_sum [out] The SHA256 sum of the certificate will be placed
 * in this buffer.
 *
 * @return true If the certificate was written successfully.
 */
bool writeECCDeviceCertificateToModem(const uint8_t slot,
                                      const bool output_certificate,
                                      char out_sha256_sum[65]) {

    size_t device_certificate_size = 0;

    int atcacert_status = ATCACERT_E_SUCCESS;

    if ((atcacert_status = ECC608.getDeviceCertificateSize(
             &device_certificate_size)) != ATCACERT_E_SUCCESS) {
        SerialModule.printf(F("Failed to get the size of the device "
                              "certificate, error code: %X.\r\n"),
                            atcacert_status);
        return false;
    }

    uint8_t device_certificate[device_certificate_size];

    if ((atcacert_status = ECC608.getDeviceCertificate(
             device_certificate,
             &device_certificate_size)) != ATCACERT_E_SUCCESS) {
        SerialModule.printf(F("Failed to get the device certificate, "
                              "error code: %X.\r\n"),
                            atcacert_status);
        return false;
    }

    // We need to base64 encode the device certificate before writing it
    // to the modem
    size_t base64_encoded_size = ECC608.calculateBase64EncodedCertificateSize(
        device_certificate_size);

    const char* base64_encoded_prefix = PSTR("-----BEGIN CERTIFICATE-----\n");
    const char* base64_encoded_suffix = PSTR("\n-----END CERTIFICATE-----\n");

    // Append prefix
    strncpy_P(data_buffer,
              base64_encoded_prefix,
              strlen_P(base64_encoded_prefix));

    ATCA_STATUS atca_status = ECC608.base64EncodeCertificate(
        device_certificate,
        device_certificate_size,
        data_buffer + strlen_P(base64_encoded_prefix),
        &base64_encoded_size);

    if (atca_status != ATCA_SUCCESS) {
        SerialModule.printf(F("Failed to encode device certificate, error "
                              "code: %X.\r\n"),
                            atca_status);
        return false;
    }

    // Append the suffix
    strncpy_P(data_buffer + strlen_P(base64_encoded_prefix) +
                  base64_encoded_size,
              base64_encoded_suffix,
              strlen_P(base64_encoded_suffix));

    ResponseResult status = writeCertificate(slot, data_buffer);

    if (status != ResponseResult::OK) {
        SerialModule.printf(F("Failed to write device certificate to "
                              "modem, error code: %X.\r\n"),
                            static_cast<uint8_t>(status));
        return false;
    }

    // Now lets generate the SHA 256 sum. The result is 32 bytes which is
    // then converted to 64 bytes of hexadecimals

    if (out_sha256_sum != NULL) {

        static uint8_t result[32];
        atcab_hw_sha2_256(device_certificate, device_certificate_size, result);

        if ((atca_status = atcab_hw_sha2_256(device_certificate,
                                             device_certificate_size,
                                             result)) != ATCA_SUCCESS) {
            SerialModule.printf(
                F("Failed to generate SHA256 thumbprint of device "
                  "certificate, error code: %X.\r\n"),
                atca_status);
            return false;
        }

        // Convert to hexadecimals
        static const char hex_conversion[] = "0123456789abcdef";

        for (uint8_t i = 0; i < sizeof(result); i++) {
            out_sha256_sum[i * 2]     = hex_conversion[(result[i] >> 4) & 0x0F];
            out_sha256_sum[i * 2 + 1] = hex_conversion[result[i] & 0x0F];
        }
    }

    if (output_certificate) {

        // We need some formatting for the certificate to appear correctly in
        // the serial terminal. We mainly want to add carriage return after the
        // prefix and after the certificate data
        //
        // These modifications are done here in order to not have to reserve
        // more space on the stack. We use some pointer arithmic to avoid having
        // to copy the data around.

        // Terminate after the certificate content to not print the suffix. At
        // this point the data buffer has already been used to store the
        // certificate, so we can freely modify it
        data_buffer[base64_encoded_size + strlen_P(base64_encoded_prefix)] =
            '\0';

        char* data_ptr = (char*)base64_encoded_prefix;
        char data      = '\0';

        // Print the prefix. We do this manually as the certificate in the data
        // buffer does not include a carriage return (which we need for the
        // formatting in the serial terminal)
        while ((data = pgm_read_byte_far(data_ptr)) != '\0') {
            SerialModule.print(data);
            data_ptr++;
        }
        SerialModule.print(F("\r"));

        // Now we jump past the prefix in the certificate data
        SerialModule.println(data_buffer + strlen_P(base64_encoded_prefix));
        SerialModule.print(F("\r"));

        // Skip the newline at the start of the suffix
        data_ptr = (char*)base64_encoded_suffix + 1;
        while ((data = pgm_read_byte_far(data_ptr)) != '\0') {
            SerialModule.print(data);
            data_ptr++;
        }
    }

    return true;
}

/**
 * @brief Builds an ASN.1 entry.
 *
 * @param entry_template The template for the ASN.1 entry, includes the object
 * type.
 * @param entry_template_length Length of template.
 * @param data The data to be injected in the entry.
 * @param data_length Length of the entry.
 * @param output_buffer Where to place the whole ASN.1 entry with the data.
 *
 * @return The size of the whole entry.
 */
size_t buildASN1Entry(const uint8_t* entry_template,
                      const size_t entry_template_length,
                      const uint8_t* data,
                      const size_t data_length,
                      uint8_t* output_buffer) {
    // First copy over the template
    memcpy(output_buffer, entry_template, entry_template_length);

    // Set the data field
    memcpy(&output_buffer[entry_template_length], data, data_length);

    const size_t total_length = entry_template_length + data_length;

    // Set the length of the ASN.1 sequence, set and data field
    output_buffer[INFO_ENTRY_SET_LENGTH_OFFSET]      = total_length - 2;
    output_buffer[INFO_ENTRY_SEQUENCE_LENGTH_OFFSET] = total_length - 4;
    output_buffer[INFO_ENTRY_DATA_LENGTH_OFFSET]     = data_length;

    return total_length;
}

/**
 * @brief Builds an ASN.1 entry where the data is retrieved from the user in the
 * form of text input.
 *
 * @param question Quesiton to ask the user for the input.
 * @param max_input_length Max user input length.
 * @param capitalize Whether the input should be capatilized.
 * @param entry_template The template for the ASN.1 entry, includes the object
 * type.
 * @param entry_template_length Length of template.
 * @param output_buffer Where to place the whole ASN.1 entry with the data.
 *
 * @return The size of the whole entry.
 */
size_t buildASN1EntryFromUserInput(const __FlashStringHelper* question,
                                   const size_t max_input_length,
                                   const bool capitalize,
                                   const uint8_t* entry_template,
                                   const size_t entry_template_length,
                                   uint8_t* output_buffer) {

    // +1 for null termination
    uint8_t data[max_input_length + 1];
    askInputQuestion(question, (char*)data, sizeof(data));

    if (capitalize) {
        for (size_t i = 0; i < sizeof(data) - 1; i++) {
            data[i] = toupper((char)data[i]);
        }
    }

    return buildASN1Entry(entry_template,
                          entry_template_length,
                          data,
                          strlen((char*)data),
                          output_buffer);
}

/**
 * @brief Wraps a series for ASN.1 entries in a ASN.1 sequence.
 *
 * @param entries Double ptr to the entries.
 * @param entries_length Array with length of each entry.
 * @param amount_of_entries  Amount of entries in the @p entries array.
 * @param out_buffer Where the whole sequence will be placed.
 *
 * @return The total length of the sequence.
 */
static size_t wrapInASN1Sequence(const uint8_t** entries,
                                 const size_t* entries_length,
                                 const size_t amount_of_entries,
                                 uint8_t* out_buffer) {

    size_t index = 0;

    // Header, support up to 256 bytes of data
    out_buffer[index++] = 0x30;
    out_buffer[index++] = 0x82;
    out_buffer[index++] = 0x0; // Dummy entry for length, will be set
    out_buffer[index++] = 0x0; // at end

    for (size_t i = 0; i < amount_of_entries; i++) {
        memcpy(&out_buffer[index], entries[i], entries_length[i]);
        index += entries_length[i];
    }

    // Then set the length minus the header
    out_buffer[2] = (index - 4) >> 8;
    out_buffer[3] = (index - 4) & 0xFF;

    return index;
}

/**
 * @brief Constructs a CSR from a template and signs it using the private key in
 * the ECC. Will ask the user for input concerning country code etc.
 *
 * @param pem Buffer to place the CSR.
 * @param pem_size The size of the buffer, will be overwritten with the final
 * size of the PEM CSR.
 *
 * @return Status code from cryptoauthlib.
 */
static ATCA_STATUS constructCSR(char* pem, size_t* pem_size) {

    static uint8_t country_code_entry[sizeof(country_code_template) + 2];

    const size_t country_code_entry_length = buildASN1EntryFromUserInput(
        F("Input country code (e.g. US). Press enter when done: "),
        2,
        true,
        country_code_template,
        sizeof(country_code_template),
        country_code_entry);

    SerialModule.println();

    static uint8_t
        state_or_province_name_entry[sizeof(state_or_province_name_template) +
                                     32];
    const size_t state_or_province_name_entry_length =
        buildASN1EntryFromUserInput(
            F("Input state or province name. Press enter when done: "),
            32,
            false,
            state_or_province_name_template,
            sizeof(state_or_province_name_template),
            state_or_province_name_entry);

    SerialModule.println();

    static uint8_t locality_name_entry[sizeof(locality_name_template) + 32];
    const size_t locality_name_entry_length = buildASN1EntryFromUserInput(
        F("Input locality name (e.g. city). Press enter when done: "),
        32,
        false,
        locality_name_template,
        sizeof(locality_name_template),
        locality_name_entry);

    // --- All user input has now finished ---

    // Retrieve the thing name from the ECC and use that as the common name
    // field
    static uint8_t common_name[128];
    size_t common_name_length = sizeof(common_name);

    if (ECC608.readProvisionItem(AWS_THINGNAME,
                                 common_name,
                                 &common_name_length) != ATCA_SUCCESS) {
        const char* default_identifier = "AVR-IoT Cellular Mini";
        common_name_length             = strlen(default_identifier);
        memcpy(common_name, default_identifier, common_name_length);
    }

    uint8_t
        common_name_entry[sizeof(common_name_template) + common_name_length];
    const size_t common_name_entry_length = sizeof(common_name_entry);

    buildASN1Entry(common_name_template,
                   sizeof(common_name_template),
                   common_name,
                   common_name_length,
                   common_name_entry);

    // Now we can build the CSR
    size_t length                                    = 0;
    size_t information_and_public_key_section_length = 0;

    // First we wrap the information fields in a sequence
    uint8_t information_fields[country_code_entry_length +
                               state_or_province_name_entry_length +
                               locality_name_entry_length +
                               sizeof(organization_name_template) +
                               common_name_entry_length + 64];

    {
        const uint8_t* entries[]      = {country_code_entry,
                                         state_or_province_name_entry,
                                         locality_name_entry,
                                         organization_name_template,
                                         common_name_entry};
        const size_t entries_length[] = {country_code_entry_length,
                                         state_or_province_name_entry_length,
                                         locality_name_entry_length,
                                         sizeof(organization_name_template),
                                         common_name_entry_length};

        length = wrapInASN1Sequence(entries,
                                    entries_length,
                                    sizeof(entries_length) /
                                        sizeof(entries_length[0]),
                                    information_fields);
    }

    // Then wrap the version header, the information fields and the public key
    // template
    uint8_t data_section[sizeof(version_template) + length +
                         sizeof(ec_public_key_template) + 64];

    {
        const uint8_t* entries[]      = {version_template,
                                         information_fields,
                                         ec_public_key_template};
        const size_t entries_length[] = {sizeof(version_template),
                                         length,
                                         sizeof(ec_public_key_template)};

        information_and_public_key_section_length = wrapInASN1Sequence(
            entries,
            entries_length,
            sizeof(entries_length) / sizeof(entries_length[0]),
            data_section);
    }

    // Then finish the whole wrap with the signature template
    uint8_t csr[length + sizeof(signature_template) + 64];
    {
        const uint8_t* entries[]      = {data_section, signature_template};
        const size_t entries_length[] = {
            information_and_public_key_section_length,
            sizeof(signature_template)};

        length = wrapInASN1Sequence(entries,
                                    entries_length,
                                    sizeof(entries_length) /
                                        sizeof(entries_length[0]),
                                    csr);
    }

    // Public key offset is computed from the following:
    //   4 (start of first sequence)
    // + length of information and public key section
    // - 19 (extension request entry including sequence and value)
    // - 68 (length of public key entry)
    // + 4 (skip past entry bytes for public key + two first bytes)
    // = length(information and and public key section) - 79

    // The signature offset is computed from the following:
    //   length of whole csr
    // - 74 (two 256 bit numbers + headers + sequence)

    const atcacert_def_t csr_definition = {
        .type               = CERTTYPE_X509,
        .template_id        = 3,
        .chain_id           = 0,
        .private_key_slot   = 0,
        .sn_source          = SNSRC_PUB_KEY_HASH,
        .cert_sn_dev_loc    = {.zone      = DEVZONE_NONE,
                               .slot      = 0,
                               .is_genkey = 0,
                               .offset    = 0,
                               .count     = 0},
        .issue_date_format  = DATEFMT_RFC5280_UTC,
        .expire_date_format = DATEFMT_RFC5280_UTC,
        .tbs_cert_loc       = {.offset = 4,
                               .count  = information_and_public_key_section_length},
        .expire_years       = 0,
        .public_key_dev_loc = {.zone      = DEVZONE_NONE,
                               .slot      = 0,
                               .is_genkey = 1,
                               .offset    = 0,
                               .count     = 64},
        .comp_cert_dev_loc  = {.zone      = DEVZONE_NONE,
                               .slot      = 0,
                               .is_genkey = 0,
                               .offset    = 0,
                               .count     = 0},
        .std_cert_elements =
            {{// STDCERT_PUBLIC_KEY
              .offset = information_and_public_key_section_length - 79,
              .count  = 64},
             {// STDCERT_SIGNATURE
              .offset = length - 74,
              .count  = 74},
             {// STDCERT_ISSUE_DATE
              .offset = 0,
              .count  = 0},
             {// STDCERT_EXPIRE_DATE
              .offset = 0,
              .count  = 0},
             {// STDCERT_SIGNER_ID
              .offset = 0,
              .count  = 0},
             {// STDCERT_CERT_SN
              .offset = 0,
              .count  = 0},
             {// STDCERT_AUTH_KEY_ID
              .offset = 0,
              .count  = 0},
             {// STDCERT_SUBJ_KEY_ID
              .offset = 0,
              .count  = 0}},
        .cert_elements       = NULL,
        .cert_elements_count = 0,
        .cert_template       = csr,
        .cert_template_size  = length,
        .ca_cert_def         = NULL};

    return (ATCA_STATUS)atcacert_create_csr_pem(&csr_definition, pem, pem_size);
}

// -----------------------------------------------------------------------------
//                                  Azure
// -----------------------------------------------------------------------------

#define DIGI_CERT_GLOBAL_ROOT_G2 (1)
#define BALTIMORE_CYBER_TRUST    (2)

const char digi_cert_global_root_g2[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
    "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
    "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
    "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
    "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
    "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
    "MrY=\n"
    "-----END CERTIFICATE-----";

const char baltimore_cyber_trust[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n"
    "RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n"
    "VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n"
    "DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n"
    "ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n"
    "VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n"
    "mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n"
    "IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n"
    "mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n"
    "XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n"
    "dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n"
    "jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n"
    "BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n"
    "DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n"
    "9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n"
    "jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n"
    "Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n"
    "ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n"
    "R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n"
    "-----END CERTIFICATE-----";

void azureIoTHubMqttProvision() {

    SerialModule.println(F("\r\n"));

    // ------------------------------------------------------------------------
    //                  Step 1: Choosing IoT Hub root CA
    // ------------------------------------------------------------------------

    //  We use the default root certificates:
    // - DigiCert Global Root G2
    // - Baltimore CyberTrust Root

    const uint8_t ca_type = askNumberedQuestion(
        F("Which root CA do you want to use? If unsure, double check in "
          "your "
          "Azure IoT Hub.\r\n"
          "1: DigiCert Global Root G2\r\n"
          "2: Baltimore CyberTrust Root\r\n"
          "Please choose (press enter when done): "),
        1,
        2);

    SerialModule.println("\r\n");

    switch (ca_type) {
    case DIGI_CERT_GLOBAL_ROOT_G2:
        strncpy_P(data_buffer,
                  digi_cert_global_root_g2,
                  sizeof(digi_cert_global_root_g2));
        break;
    case BALTIMORE_CYBER_TRUST:
        strncpy_P(data_buffer,
                  baltimore_cyber_trust,
                  sizeof(baltimore_cyber_trust));
        break;

    default:
        break;
    }

    const ResponseResult certificate_write_result =
        writeCertificate(MQTT_CUSTOM_CA_SLOT, data_buffer);

    if (certificate_write_result != ResponseResult::OK) {
        SerialModule.printf(F("Error occurred whilst storing CA "
                              "certificate, error code: %X."),
                            static_cast<uint8_t>(certificate_write_result));
        return;
    }

    // ------------------------------------------------------------------------
    //     Step 2: Store the IoT Hub hostname and the device ID in the ECC
    // ------------------------------------------------------------------------

    static char hostname[256] = "";
    askInputQuestion(
        F("Input Azure IoT Hub hostname, it should be on the form: "
          "<your-iot-hub-name>.azure-devices.net\r\nPress enter when done: "),
        hostname,
        sizeof(hostname) - 1);

    SerialModule.println("\r\n");

    ATCA_STATUS atca_status = ATCA_SUCCESS;

    if ((atca_status = ECC608.begin()) != ATCA_SUCCESS) {
        SerialModule.printf(F("Failed to initialize the ECC608 to store the "
                              "Azure IoT hostname, error code: %X.\r\n"),
                            atca_status);
        return;
    }

    // The device ID is the common name field of the device certificate, which
    // consists of the serial number, so we don't need to query that

    // The serial number is 9 bytes
    static uint8_t serial_number[9];

    if ((atca_status = atcab_read_serial_number(serial_number)) !=
        ATCA_SUCCESS) {
        SerialModule.printf(F("Failed to read the serial number from the "
                              "ECC, error code: %X\r\n"),
                            atca_status);
        return;
    }

    // Common name in the device certificate is given by snXXXXXXXXXXXXXXXXXX,
    // where every pair of XX is the hexidecimal of the serial number digits
    //
    // We add 2 here to have place for the "sn" and 1 for NULL termination.
    static char common_name[18 + 2 + 1] = "";

    snprintf_P(common_name,
               sizeof(common_name),
               PSTR("sn%02X%02X%02X%02X%02X%02X%02X%02X%02X"),
               serial_number[0],
               serial_number[1],
               serial_number[2],
               serial_number[3],
               serial_number[4],
               serial_number[5],
               serial_number[6],
               serial_number[7],
               serial_number[8]);

    const enum ecc_data_types types[2] = {AZURE_IOT_HUB_NAME, AZURE_DEVICE_ID};
    const char* provision_data[2]      = {hostname, common_name};
    const size_t provision_data_sizes[2] = {strlen(hostname),
                                            strlen(common_name)};

    atca_status = ECC608.writeProvisionData(2,
                                            types,
                                            (const uint8_t**)provision_data,
                                            provision_data_sizes);

    if (atca_status != ATCA_SUCCESS) {
        SerialModule.printf(
            F("Failed to write the provision data to the ECC608, "
              "error code: %X\r\n"),
            atca_status);
        return;
    }

    // ------------------------------------------------------------------------
    //                  Step 3: Writing security profile
    // ------------------------------------------------------------------------

    // We use TLS 1.2 for Azure IoT Hub and the recommended ciphers:
    //
    // - TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256: 0xC02F
    // - TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384: 0xC030
    // - TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256: 0xC027
    // - TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384: 0xC028
    snprintf_P(command_buffer,
               sizeof(command_buffer),
               AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC,
               2,
               "0xC02F;0xC030;0xC027;0xC028",
               1,
               MQTT_CUSTOM_CA_SLOT,
               MQTT_PUBLIC_KEY_SLOT,
               MQTT_PRIVATE_KEY_SLOT,
               "",
               "");

    SequansController.writeBytes((uint8_t*)command_buffer,
                                 strlen(command_buffer),
                                 true);

    // Wait for URC confirming the security profile
    if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
        SerialModule.println(F("Error whilst writing the security profile"));
        return;
    }

    // ------------------------------------------------------------------------
    //              Step 4: Write device certificate to modem
    // ------------------------------------------------------------------------

    // We need to generate the SHA256 sum of the device certificate for the user
    // to input into Azure IoT Hub as well as storing the device certificate in
    // the modem.
    char device_certificate_sha256[65] = "";

    if (!writeECCDeviceCertificateToModem(MQTT_PUBLIC_KEY_SLOT,
                                          false,
                                          device_certificate_sha256)) {
        return;
    }

    // ------------------------------------------------------------------------
    //         Step 5: Print out Azure IoT Hub instructions for user
    // ------------------------------------------------------------------------

    // The user needs to use the common name of the device certificate as the
    // Azure IoT Hub device ID and the SHA256 sum of the device certificate as
    // the thumbprints

    SerialModule.printf(
        F("The provisioning is now complete on the device part. "
          "Please follow the instructions\r\nbelow to complete the "
          "provisioning on the Azure IoT Hub side:\r\n"
          "1. Go to the Azure IoT Hub portal and select your "
          "IoT Hub\r\n"
          "2. Select \"Devices\" in the menu to the left "
          "and then click \"Add Device\"\r\n"
          "3. Input the following as the device ID (this is the common name "
          "of "
          "the ECC device certificate): %s\r\n"
          "4. Select \"X.509 Self-Signed\" and input the following as the "
          "primary and secondary thumbprint "
          "(this "
          "is the SHA256 sum of the device certificate): %s\r\n"),
        common_name,
        device_certificate_sha256);

    SerialModule.println();

    // Dummy question to make sure user reads the instructions
    askInputQuestion(F("Press enter to return to the main menu"), nullptr, 0);
}

// -----------------------------------------------------------------------------
//                                  AWS
// -----------------------------------------------------------------------------

void awsMqttProvision() {
    SerialModule.println();

    // The user should use iotprovisiontool for AWS

    SerialModule.println(
        F("\r\nPlease use the iotprovisiontool to provision the "
          "device for AWS.\r\nSee more details under user guide on "
          "iot.microchip.com/docs"));

    SerialModule.println();

    askInputQuestion(F("Press enter to return to the main menu"), nullptr, 0);
}

// -----------------------------------------------------------------------------
//                          Other MQTT services
// -----------------------------------------------------------------------------

void otherServiceMqttProvision() {

    SerialModule.println(F("\r\n"));

    // ------------------------------------------------------------------------
    //              Step 1: Choosing authentication method
    // ------------------------------------------------------------------------

    const uint8_t method = askNumberedQuestion(
        F("Which method do you want to use?\r\n"
          "1: MQTT TLS unauthenticated or with username and password\r\n"
          "2: MQTT TLS with public and private key pair certificates\r\n"
          "Please choose (press enter when done): "),
        1,
        2);

    SerialModule.println(F("\r\n"));

    // ------------------------------------------------------------------------
    //                      Step 2: Choosing TLS version
    // ------------------------------------------------------------------------

    uint8_t tls_version = askNumberedQuestion(
        F("Which TLS version do you want to use?\r\n"
          "1: TLS 1\r\n"
          "2: TLS 1.1\r\n"
          "3: TLS 1.2 (default)\r\n"
          "4: TLS 1.3\r\n"
          "Please choose (press enter when done): "),
        1,
        4);

    // We present the choices for TLS 1 indexed for the user, but the modem uses
    // 0 indexed
    tls_version -= 1;

    SerialModule.println(F("\r\n"));

    // ------------------------------------------------------------------------
    //                      Step 3: Choosing ciphers
    // ------------------------------------------------------------------------

    bool has_chosen_ciphers                       = false;
    static bool ciphers_chosen[NUMBER_OF_CIPHERS] = {};
    bool chose_automatic_cipher_selection         = true;

    // Each entry takes up 6 characters + delimiter, +1 at end for NULL
    // termination
    static char ciphers[(6 + 1) * NUMBER_OF_CIPHERS + 1] = "";
    size_t number_of_ciphers_chosen                      = 0;

    static char psk[64]          = "";
    static char psk_identity[64] = "";

    while (!has_chosen_ciphers) {

        number_of_ciphers_chosen = 0;

        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS; i++) {
            ciphers_chosen[i] = false;
        }

        SerialModule.println(
            F("Which cipher(s) do you want to use? Leave blank for the "
              "cipher to be chosen automatically.\r\nSelect "
              "multiple ciphers by comma between the indices (e.g. 1,2,3 for "
              "the first three ciphers)"));

        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS / 2; i++) {

            // Buffer to load from progmem
            char cipher_text_value_left[CIPHER_TEXT_MAX_LENGTH]  = "";
            char cipher_text_value_right[CIPHER_TEXT_MAX_LENGTH] = "";

            strcpy_P(cipher_text_value_left,
                     (PGM_P)pgm_read_word_far(&(cipher_text_values[i])));

            strcpy_P(cipher_text_value_right,
                     (PGM_P)pgm_read_word_far(
                         &(cipher_text_values[NUMBER_OF_CIPHERS / 2 + i])));

            snprintf_P(data_buffer,
                       sizeof(data_buffer),
                       PSTR("%2d: %-52s %2d: %-52s"),
                       i + 1,
                       cipher_text_value_left,
                       NUMBER_OF_CIPHERS / 2 + i + 1,
                       cipher_text_value_right);

            SerialModule.println(data_buffer);
        }

        SerialModule.print(
            F("\r\nPlease choose. Press enter when done (leave blank "
              "for cipher to be chosen automatically): "));

        memset(data_buffer, '\0', sizeof(data_buffer));

        if (!readUntilNewLine(data_buffer, sizeof(data_buffer))) {
            SerialModule.println(F("\r\nInput too long!"));
            continue;
        }

        has_chosen_ciphers = true;

        // User did not leave the choices blank for automatic cipher
        // selection within the modem, parse them
        if (strlen(data_buffer) > 0) {
            chose_automatic_cipher_selection = false;

            // Split the input to go through each cipher selected
            char* ptr;
            ptr = strtok(data_buffer, ",");

            while (ptr != NULL) {

                // Attempt to convert to number
                uint8_t cipher_index = strtol(ptr, NULL, 10);

                if (cipher_index != 0 && cipher_index <= NUMBER_OF_CIPHERS) {
                    // Choices in the serial input are 1 indexed, so do -1
                    ciphers_chosen[cipher_index - 1] = true;
                } else {
                    SerialModule.print(F("\r\nUnknown cipher selected: "));
                    SerialModule.println(ptr);
                    has_chosen_ciphers = false;
                    break;
                }

                ptr = strtok(NULL, ",");
            }
        }
    }

    if (chose_automatic_cipher_selection) {
        SerialModule.println(F("\r\nCipher will be detected automatically"));
    } else {
        SerialModule.println(F("\r\nCiphers chosen:"));

        // First just record how many ciphers that were chosen
        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS; i++) {
            if (ciphers_chosen[i]) {
                number_of_ciphers_chosen++;
            }
        }

        bool has_chosen_psk_cipher    = false;
        size_t cipher_count           = 0;
        size_t cipher_character_index = 0;

        // Then do the actual processing of the ciphers
        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS; i++) {

            if (ciphers_chosen[i]) {

                // Append the cipher to the string which will be passed with the
                // command to the modem
                strcpy_P(&ciphers[cipher_character_index],
                         (PGM_P)pgm_read_word_far(&(cipher_values[i])));
                cipher_character_index += CIPHER_VALUE_LENGTH;
                cipher_count++;

                // Add delimiter between ciphers
                if (cipher_count < number_of_ciphers_chosen) {
                    ciphers[cipher_character_index++] = ';';
                }

                // Load from progmem before checking for PSK cipher
                char cipher_text_value[CIPHER_TEXT_MAX_LENGTH] = "";

                strcpy_P(cipher_text_value,
                         (PGM_P)pgm_read_word_far(&(cipher_text_values[i])));

                // Check if the cipher chosen is a pre shared key one, so that
                // the user has to provide the PSK and the PSK identity
                if (!has_chosen_psk_cipher &&
                    strstr_P(cipher_text_value, PSTR("PSK")) != NULL) {
                    has_chosen_psk_cipher = true;
                }

                // +1 here since the choices are presented 1 indexed for the
                // user
                snprintf_P(data_buffer,
                           sizeof(data_buffer),
                           PSTR("%2d: %-52s"),
                           i + 1,
                           cipher_text_value);
                SerialModule.println(data_buffer);
            }
        }

        // --------------------------------------------------------------------
        //              Step 3.5: Check if PSK ciphers are used
        // --------------------------------------------------------------------
        if (has_chosen_psk_cipher) {

            SerialModule.println(F("\r\nYou have chosen a pre-shared key (PSK) "
                                   "cipher\r\n"));

            while (true) {
                SerialModule.print(
                    F("Please input the PSK (press enter when done): "));

                if (!readUntilNewLine(psk, sizeof(psk))) {
                    SerialModule.println(F("\r\nInput too long!"));
                } else {
                    break;
                }
            }

            SerialModule.println();

            while (true) {
                SerialModule.print(F("Please input the PSK identity (press "
                                     "enter when done): "));

                if (!readUntilNewLine(psk_identity, sizeof(psk_identity))) {
                    SerialModule.println(F("\r\nInput too long!"));
                } else {
                    break;
                }
            }

            SerialModule.println();
        }
    }

    SerialModule.println();

    uint8_t ca_index = DEFAULT_CA_SLOT;

    // --------------------------------------------------------------------
    //                      Step 4: Custom CA
    // --------------------------------------------------------------------

    bool load_custom_ca = askCloseEndedQuestion(
        F("Do you want to load a custom certificate authority "
          "certificate?"));

    if (load_custom_ca) {
        ca_index = MQTT_CUSTOM_CA_SLOT;

        SerialModule.println("\r\n");

        while (!requestAndSaveToNVM(
            F("Please paste in the CA certificate and press enter. It "
              "should be on the following form:\r\n"
              "-----BEGIN CERTIFICATE-----\r\n"
              "MIIDXTCCAkWgAwIBAgIJAJC1[...]j3tCx2IUXVqRs5mlSbvA==\r\n"
              "-----END CERTIFICATE-----\r\n"),
            ca_index,
            true)) {
            SerialModule.println("\r\n");
        }
    } else {
        SerialModule.println();
    }

    if (method == MQTT_TLS) {

        // --------------------------------------------------------------------
        //                  Step 5: Write the security profile
        // --------------------------------------------------------------------

        // For regular TLS without public and private key pair we're done now,
        // just need to write the command
        SerialModule.print(F("\r\nProvisioning..."));

        snprintf_P(command_buffer,
                   sizeof(command_buffer),
                   AT_MQTT_SECURITY_PROFILE,
                   tls_version,
                   ciphers,
                   1,
                   ca_index,
                   psk,
                   psk_identity);

        SequansController.writeBytes((uint8_t*)command_buffer,
                                     strlen(command_buffer),
                                     true);

        // Wait for URC confirming the security profile
        if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
            SerialModule.print(F("Error whilst doing the provisioning"));
        } else {
            SerialModule.print(F(" Done!"));
        }

    } else {

        SerialModule.println();

        const bool use_ecc = askCloseEndedQuestion(
            F("Do you want to utilize the ECC cryptography chip rather than "
              "storing the certificates in the non-volatile memory of the "
              "modem?"));

        SerialModule.println("\r\n");

        if (use_ecc) {

            const bool do_csr = askCloseEndedQuestion(
                F("Do you need to do a CSR (Certificate Signing "
                  "Request)?\r\nIf your broker allows for uploading the device "
                  "certificate directly, you can skip this step."));

            SerialModule.println("\r\n");

            ATCA_STATUS atca_status = ATCA_SUCCESS;

            if ((atca_status = ECC608.begin()) != ATCA_SUCCESS) {
                SerialModule.printf(
                    F("Failed to initialize the ECC608, error code: %X.\r\n"),
                    atca_status);
                return;
            }

            if (do_csr) {

                // -------------------------------------------------------------
                //      Step 5: Create the CSR which is given to the broker
                // -------------------------------------------------------------

                size_t pem_size = sizeof(data_buffer);

                ATCA_STATUS status = constructCSR(data_buffer, &pem_size);

                if (status != ATCA_SUCCESS) {
                    SerialModule.print(F("Failed to create CSR, error code: "));
                    SerialModule.println(status);
                    return;
                }

                SerialModule.println(
                    F("\r\n\r\nPlease use the following CSR and sign it "
                      "with your MQTT broker:\r\n"));

                SerialModule.println(data_buffer);
                SerialModule.println("\r\n");

                // -------------------------------------------------------------
                //   Step 6: Save the certificate which is provided by the MQTT
                //                  broker after using the CSR
                // -------------------------------------------------------------

                while (!requestAndSaveToNVM(
                    F("Please paste in the public key certificate provide by "
                      "your broker after having signed the CSR\r\nand press "
                      "enter. It should be on the following form:\r\n"
                      "-----BEGIN CERTIFICATE-----\r\n"
                      "MIIDXTCCAkWgAwIBAgIJAJC1[...]j3tCx2IUXVqRs5mlSbvA=="
                      "\r\n"
                      "-----END CERTIFICATE-----\r\n"),
                    MQTT_PUBLIC_KEY_SLOT,
                    true)) {
                    SerialModule.println("\r\n");
                }

                // -------------------------------------------------------------
                //            Step 7: Write the security profile
                // -------------------------------------------------------------

                SerialModule.print(F("\r\n\r\nProvisioning..."));

                snprintf_P(command_buffer,
                           sizeof(command_buffer),
                           AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC,
                           tls_version,
                           ciphers,
                           1,
                           ca_index,
                           MQTT_PUBLIC_KEY_SLOT,
                           MQTT_PRIVATE_KEY_SLOT,
                           psk,
                           psk_identity);

                SequansController.writeBytes((uint8_t*)command_buffer,
                                             strlen(command_buffer),
                                             true);

                // Wait for URC confirming the security profile
                if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
                    SerialModule.print(
                        F(" Error whilst doing the provisioning"));
                } else {
                    SerialModule.print(F(" Done!"));
                }
            } else {
                // -------------------------------------------------------------
                //      Step 5: Save ECC device certificate to modem
                // -------------------------------------------------------------

                if (!writeECCDeviceCertificateToModem(MQTT_PUBLIC_KEY_SLOT,
                                                      true,
                                                      nullptr)) {
                    return;
                }

                SerialModule.println(
                    F("\r\nPlease use the device certificate "
                      "provided above to authenticate with your "
                      "MQTT broker.\r\n"));

                // -------------------------------------------------------------
                //            Step 6: Write the security profile
                // -------------------------------------------------------------

                SerialModule.print(F("\r\nProvisioning..."));

                snprintf_P(command_buffer,
                           sizeof(command_buffer),
                           AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC,
                           tls_version,
                           ciphers,
                           1,
                           ca_index,
                           MQTT_PUBLIC_KEY_SLOT,
                           MQTT_PRIVATE_KEY_SLOT,
                           psk,
                           psk_identity);

                SequansController.writeBytes((uint8_t*)command_buffer,
                                             strlen(command_buffer),
                                             true);

                // Wait for URC confirming the security profile
                if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
                    SerialModule.print(
                        F(" Error whilst doing the provisioning"));
                } else {
                    SerialModule.print(F(" Done!"));
                }
            }

        } else {

            // -----------------------------------------------------------------
            //           Step 5: Load user's certificate and private key
            // -----------------------------------------------------------------

            while (!requestAndSaveToNVM(
                F("Please paste in the public key certificate and press "
                  "enter. It "
                  "should be on the following form:\r\n"
                  "-----BEGIN CERTIFICATE-----\r\n"
                  "MIIDXTCCAkWgAwIBAgIJAJC1[...]j3tCx2IUXVqRs5mlSbvA==\r\n"
                  "-----END CERTIFICATE-----\r\n"),
                MQTT_PUBLIC_KEY_SLOT,
                true)) {
                SerialModule.println("\r\n");
            }

            SerialModule.println("\r\n");
            while (!requestAndSaveToNVM(
                F("Please paste in the private key and press enter. "
                  "It should be on the following form:\r\n"
                  "-----BEGIN RSA/EC PRIVATE KEY-----\r\n"
                  "...\r\n"
                  "-----END RSA/EC PRIVATE KEY-----\r\n"),
                MQTT_PRIVATE_KEY_SLOT,
                false)) {
                SerialModule.println("\r\n");
            }

            // -----------------------------------------------------------------
            //                 Step 6: Write the security profile
            // -----------------------------------------------------------------

            SerialModule.print(F("\r\n\r\nProvisioning..."));

            snprintf_P(command_buffer,
                       sizeof(command_buffer),
                       AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES,
                       tls_version,
                       ciphers,
                       1,
                       ca_index,
                       MQTT_PUBLIC_KEY_SLOT,
                       MQTT_PRIVATE_KEY_SLOT,
                       psk,
                       psk_identity);

            SequansController.writeBytes((uint8_t*)command_buffer,
                                         strlen(command_buffer),
                                         true);

            // Wait for URC confirming the security profile
            if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
                SerialModule.print(F(" Error whilst doing the provisioning"));
            } else {
                SerialModule.print(F(" Done!"));
            }
        }

        SequansController.clearReceiveBuffer();
    }

    SerialModule.println(F("\r\n"));

    askInputQuestion(F("Press enter to return to the main menu"), nullptr, 0);
}

void provisionMqtt() {

    SerialModule.println("\r\n");

    const uint8_t service_choice = askNumberedQuestion(
        F("Which service do you want to use?\r\n"
          "1: Azure IoT Hub\r\n"
          "2: AWS\r\n"
          "3: Other\r\n"
          "Please choose (press enter when done): "),
        1,
        3);

    Service service = static_cast<Service>(service_choice);

    switch (service) {
    case Service::AzureIoTHub:
        azureIoTHubMqttProvision();
        break;

    case Service::AWS:
        awsMqttProvision();
        break;

    case Service::Other:
        otherServiceMqttProvision();
        break;
    }
}

void provisionHttp() {

    SerialModule.println("\r\n");

    // ------------------------------------------------------------------------
    //                      Step 1: Choosing TLS version
    // ------------------------------------------------------------------------

    uint8_t tls_version = askNumberedQuestion(
        F("Which TLS version do you want to use?\r\n"
          "1: TLS 1\r\n"
          "2: TLS 1.1\r\n"
          "3: TLS 1.2 (default)\r\n"
          "4: TLS 1.3\r\n"
          "Please choose (press enter when done): "),
        1,
        4);

    // We present the choices for TLS 1 indexed for the user, but the modem
    // uses 0 indexed, so just substract
    tls_version -= 1;

    SerialModule.println("\r\n");

    // --------------------------------------------------------------------
    //                            Step 2: Custom CA
    // --------------------------------------------------------------------

    uint8_t ca_index = DEFAULT_CA_SLOT;

    bool load_custom_ca = askCloseEndedQuestion(
        F("Do you want to load a custom certificate authority "
          "certificate?"));

    if (load_custom_ca) {
        ca_index = HTTP_CUSTOM_CA_SLOT;

        SerialModule.println("\r\n");

        while (!requestAndSaveToNVM(
            F("Please paste in the CA certificate and press enter. It "
              "should "
              "be on the following form:\r\n"
              "-----BEGIN CERTIFICATE-----\r\n"
              "MIIDXTCCAkWgAwIBAgIJAJC1[...]j3tCx2IUXVqRs5mlSbvA=="
              "\r\n"
              "-----END CERTIFICATE-----\r\n"),
            ca_index,
            true)) {
            SerialModule.println("\r\n");
        }
    } else {
        SerialModule.println();
    }

    // --------------------------------------------------------------------
    //                  Step 3: Write the security profile
    // --------------------------------------------------------------------

    SerialModule.print(F("\r\nProvisioning..."));

    char command[strlen_P(AT_HTTPS_SECURITY_PROFILE) + 64] = "";

    snprintf_P(command,
               sizeof(command),
               AT_HTTPS_SECURITY_PROFILE,
               tls_version,
               1,
               ca_index);

    SequansController.writeBytes((uint8_t*)command, strlen(command), true);

    // Wait for URC confirming the security profile
    if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
        SerialModule.print(F(" Error whilst doing the provisioning."));
    } else {
        SerialModule.print(F(" Done!"));
    }

    SequansController.clearReceiveBuffer();

    SerialModule.println(F("\r\n"));

    askInputQuestion(F("Press enter to return to the main menu"), nullptr, 0);
}

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    SerialModule.begin(115200);
    SerialModule.setTimeout(120000);

    SequansController.begin();
}

void loop() {

    SerialModule.println(F("\r\n\r\n\r\n========= Provisioning for AVR-IoT "
                           "Cellular Mini =========\r\n"));

    SerialModule.println(
        F("Note: Please use PuTTY or an equivalent serial terminal\r\n"
          "for provisioning as especially the serial monitor in\r\n"
          "Arduino IDE 2 does not handle inputting multi-line\r\n"
          "strings well.\r\n"));

    const uint8_t provision_type = askNumberedQuestion(
        F("Method to provision\r\n"
          "1: MQTT\r\n"
          "2: HTTP\r\n"
          "Please choose (press enter when done): "),
        1,
        2);

    switch (provision_type) {
    case 1:
        provisionMqtt();
        break;

    case 2:
        provisionHttp();
        break;

    default:
        break;
    }
}
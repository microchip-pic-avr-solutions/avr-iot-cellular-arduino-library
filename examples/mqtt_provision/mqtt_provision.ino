/**
 * @brief This provisioning is for using MQTT with TLS and without the ECC. In
 * order to start the provisioning, just upload this sketch to the board.
 */

#include "led_ctrl.h"
#include "log.h"
#include "sequans_controller.h"

#define MQTT_TLS                         (1)
#define MQTT_TLS_PUBLIC_PRIVATE_KEY_PAIR (2)

#define TLS_VERSION_1   (1)
#define TLS_VERSION_1_1 (2)
#define TLS_VERSION_1_2 (3)
#define TLS_VERSION_1_3 (4)

#define AT_SETUP_SECURITY_PROFILE "AT+SQNSPCFG=2,%u,\"%s\",%u,1,,,\"%s\",\"%s\""
#define AT_SETUP_SECURITY_PROFILE_WITH_CERTIFICATES \
    "AT+SQNSPCFG=2,%u,\"%s\",%u,1,17,17,\"%s\",\"%s\""

#define AT_WRITE_CERTIFICATE "AT+SQNSVW=\"certificate\",17,%u"
#define AT_WRITE_PRIVATE_KEY "AT+SQNSVW=\"certificate\",17,%u"

#define NUMBER_OF_CIPHERS      (64)
#define CIPHER_TEXT_MAX_LENGTH (50)

#define ASCII_ENTER (13)

const char cipher_values[NUMBER_OF_CIPHERS][8] = {
    "0x1301;", // TLS_AES_128_GCM_SHA256
    "0x1302;", // TLS_AES_256_GCM_SHA384
    "0x1303;", // TLS_CHACHA20_POLY1305_SHA256
    "0x1304;", // TLS_AES_128_CCM_SHA256
    "0x1305;", // TLS_AES_128_CCM_8_SHA256
    "0x000A;", // SSL_RSA_WITH_3DES_EDE_CBC_SHA
    "0x002F;", // TLS_RSA_WITH_AES_128_CBC_SHA
    "0x0035;", // TLS_RSA_WITH_AES_256_CBC_SHA
    "0x0033;", // TLS_DHE_RSA_WITH_AES_128_CBC_SHA
    "0x0039;", // TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    "0x00AB;", // TLS_DHE_PSK_WITH_AES_256_GCM_SHA384
    "0x00AA;", // TLS_DHE_PSK_WITH_AES_128_GCM_SHA256
    "0x00A9;", // TLS_PSK_WITH_AES_256_GCM_SHA384
    "0x00A8;", // TLS_PSK_WITH_AES_128_GCM_SHA256
    "0x00B3;", // TLS_DHE_PSK_WITH_AES_256_CBC_SHA384
    "0x00B2;", // TLS_DHE_PSK_WITH_AES_128_CBC_SHA256
    "0x00AF;", // TLS_PSK_WITH_AES_256_CBC_SHA384
    "0x00AE;", // TLS_PSK_WITH_AES_128_CBC_SHA256
    "0x008C;", // TLS_PSK_WITH_AES_128_CBC_SHA
    "0x008D;", // TLS_PSK_WITH_AES_256_CBC_SHA
    "0xC0A6;", // TLS_DHE_PSK_WITH_AES_128_CCM
    "0xC0A7;", // TLS_DHE_PSK_WITH_AES_256_CCM
    "0xC0A4;", // TLS_PSK_WITH_AES_128_CCM
    "0xC0A5;", // TLS_PSK_WITH_AES_256_CCM
    "0xC0A8;", // TLS_PSK_WITH_AES_128_CCM_8
    "0xC0A9;", // TLS_PSK_WITH_AES_256_CCM_8
    "0xC0A0;", // TLS_RSA_WITH_AES_128_CCM_8
    "0xC0A1;", // TLS_RSA_WITH_AES_256_CCM_8
    "0xC0AC;", // TLS_ECDHE_ECDSA_WITH_AES_128_CCM
    "0xC0AE;", // TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8
    "0xC0AF;", // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
    "0xC014;", // TLS_EDHE_ECDSA_WITH_AES_256_CCM_8
    "0xC013;", // TLS_ECCDHE_RSA_WITH_AES_256_CBC_SHA
    "0xC009;", // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
    "0xC00A;", // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
    "0xC012;", // TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
    "0xC008;", // TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
    "0x003C;", // TLS_RSA_WITH_AES_128_CBC_SHA256
    "0x003D;", // TLS_RSA_WITH_AES_256_CBC_SHA256
    "0x0067;", // TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
    "0x006B;", // TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
    "0x009C;", // TLS_RSA_WITH_AES_128_GCM_SHA256
    "0x009D;", // TLS_RSA_WITH_AES_256_GCM_SHA384
    "0x009E;", // TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
    "0x009F;", // TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
    "0xC02F;", // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
    "0xC030;", // TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
    "0xC02B;", // TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
    "0xC02C;", // TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
    "0xC027;", // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
    "0xC023;", // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
    "0xC028;", // TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
    "0xC024;", // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
    "0xCCA8;", // TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
    "0xCCA9;", // TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
    "0xCCAA;", // TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256
    "0xCC13;", // TLS_ECDHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256
    "0xCC14;", // TLS_ECDHE_ECDSA_WITH_CHACHA20_OLD_POLY1305_SHA256
    "0xCC15;", // TLS_DHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256
    "0xC037;", // TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
    "0xCCAB;", // TLS_PSK_WITH_CHACHA20_POLY1305_SHA256
    "0xCCAC;", // TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256
    "0xCCAD;", // TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256
    "0x0016;", // TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA
};

const char cipher_text_values[NUMBER_OF_CIPHERS][CIPHER_TEXT_MAX_LENGTH] = {
    "TLS_AES_128_GCM_SHA256",
    "TLS_AES_256_GCM_SHA384",
    "TLS_CHACHA20_POLY1305_SHA256",
    "TLS_AES_128_CCM_SHA256",
    "TLS_AES_128_CCM_8_SHA256",
    "SSL_RSA_WITH_3DES_EDE_CBC_SHA",
    "TLS_RSA_WITH_AES_128_CBC_SHA",
    "TLS_RSA_WITH_AES_256_CBC_SHA",
    "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",
    "TLS_DHE_RSA_WITH_AES_256_CBC_SHA",
    "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384",
    "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256",
    "TLS_PSK_WITH_AES_256_GCM_SHA384",
    "TLS_PSK_WITH_AES_128_GCM_SHA256",
    "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384",
    "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256",
    "TLS_PSK_WITH_AES_256_CBC_SHA384",
    "TLS_PSK_WITH_AES_128_CBC_SHA256",
    "TLS_PSK_WITH_AES_128_CBC_SHA",
    "TLS_PSK_WITH_AES_256_CBC_SHA",
    "TLS_DHE_PSK_WITH_AES_128_CCM",
    "TLS_DHE_PSK_WITH_AES_256_CCM",
    "TLS_PSK_WITH_AES_128_CCM",
    "TLS_PSK_WITH_AES_256_CCM",
    "TLS_PSK_WITH_AES_128_CCM_8",
    "TLS_PSK_WITH_AES_256_CCM_8",
    "TLS_RSA_WITH_AES_128_CCM_8",
    "TLS_RSA_WITH_AES_256_CCM_8",
    "TLS_ECDHE_ECDSA_WITH_AES_128_CCM",
    "TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8",
    "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",
    "TLS_EDHE_ECDSA_WITH_AES_256_CCM_8",
    "TLS_ECCDHE_RSA_WITH_AES_256_CBC_SHA",
    "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA",
    "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
    "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA",
    "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA",
    "TLS_RSA_WITH_AES_128_CBC_SHA256",
    "TLS_RSA_WITH_AES_256_CBC_SHA256",
    "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256",
    "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256",
    "TLS_RSA_WITH_AES_128_GCM_SHA256",
    "TLS_RSA_WITH_AES_256_GCM_SHA384",
    "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256",
    "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384",
    "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
    "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
    "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
    "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
    "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",
    "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256",
    "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",
    "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384",
    "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
    "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
    "TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
    "TLS_ECDHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256",
    "TLS_ECDHE_ECDSA_WITH_CHACHA20_OLD_POLY1305_SHA256",
    "TLS_DHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256",
    "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256",
    "TLS_PSK_WITH_CHACHA20_POLY1305_SHA256",
    "TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
    "TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
    "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA"};

// TODO: Would be nice to get this to work with the ECC as well...

/*
 * Flow
 *
 * 1. Choose authentication method
 * 2. Choose the TLS version
 * 3. Choose ciphers
 *    -> If PSK cipher is used, ask about PSK and PSK identity
 * 4. Choose whether to verify the domain using a certificate authority (CA).
 *    -> Ask if a custom CA should be loaded
 * 5. If public private key pair is utilized, fill in the certificates which
 * will be written to the device.
 */

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Serial3.begin(115200);
    Serial3.setTimeout(120000);

    SequansController.begin();

    Serial3.println("=== MQTT provisioning for TLS ===");

    uint8_t method = 0;

    // Choosing authentication method
    while (true) {
        Serial3.println("\r\nWhich method do you want to use?");
        Serial3.println(
            "1: MQTT TLS unauthenticated or with username and password");
        Serial3.println(
            "2: MQTT TLS with public and private key pair certificates");
        Serial3.print("Please choose: ");

        while (Serial3.available() == 0) {}

        method = Serial3.read();
        Serial3.print((char)method);

        // Convert to number
        method = method - '0';

        if (method != MQTT_TLS && method != MQTT_TLS_PUBLIC_PRIVATE_KEY_PAIR) {
            Serial3.println("\r\nUnknown method chosen");
        } else {
            break;
        }
    }

    Serial3.println();

    bool has_chosen_ciphers = false;

    // Choosing ciphers
    while (!has_chosen_ciphers) {
        Serial3.println(
            "\r\nWhich cipher(s) do you want to use? Leave blank for the "
            "cipher to be chosen automatically. Select "
            "multiple ciphers by comma between the indices (e.g. 1,2,3 for "
            "the first three ciphers)");

        char line[164] = "";

        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS / 2; i++) {

            sprintf(line,
                    "%2d: %-52s %2d: %-52s",
                    i + 1,
                    cipher_text_values[i],
                    NUMBER_OF_CIPHERS / 2 + i + 1,
                    cipher_text_values[NUMBER_OF_CIPHERS / 2 + i]);

            Serial3.println(line);
        }

        Serial3.print("Please choose. Press enter when done: ");

        char cipher_input[128] = "";
        size_t index           = 0;

        while (true) {
            if (Serial3.available()) {
                char input = Serial3.read();

                if ((uint8_t)input == ASCII_ENTER) {
                    break;
                }

                cipher_input[index++] = input;

                Serial3.print(input);
            }
        }

        // Check input
        has_chosen_ciphers = true;

        Serial3.println();

        Serial3.print("Ciphers chosen: ");
        Serial3.println(cipher_input);
    }

    while (1) {}

    /*

    // Constructing the ciphers from the ones uncommented (if any)
    char ciphers_used[576] = "";

    // TODO: test with only one, two and all
    if (sizeof(ciphers) > 0) {
        char* cipher_used_ptr = ciphers_used;

        for (size_t i = 0; i < sizeof(ciphers) / sizeof(ciphers[0]); i++) {

            // Not all ciphers have equal length, so we calculate this here, but
            // each entry has the same amount of allocated space for it in the
            // ciphers array
            const size_t cipher_size = strlen(ciphers[i]);
            memcpy(cipher_used_ptr, ciphers[i], cipher_size);
            cipher_used_ptr += cipher_size;
        }

        // Remove the last semi-colon
        *(cipher_used_ptr - 1) = '\0';
    }

    char command_buffer[712] = "";

    switch (AUTHENTICATION_METHOD) {

    // Same provisioning for unauthenticated and username+password as the
    // username and password is provided when the MQTT client is configured
    case MQTT_TLS_UNAUTHENTICATED:
    case MQTT_TLS_USERNAME_PASSWORD:
        sprintf(command_buffer,
                AT_SETUP_SECURITY_PROFILE,
                TLS_VERSION,
                ciphers_used,
                USE_CA ? 1 : 0,
                pre_shared_key,
                pre_shared_key_identifier);

        Log.infof("Sending command: %s\r\n", command_buffer);
        SequansController.writeBytes((uint8_t*)command_buffer,
                                     strlen(command_buffer),
                                     true);
        break;

    case MQTT_TLS_PUBLIC_PRIVATE_KEY_PAIR:

        // Write certificate and private key first before setting the security
        // profile
        char certificate_command[64] = "";

        const size_t certificate_length = strlen(client_certificate);
        sprintf(certificate_command, AT_WRITE_CERTIFICATE, certificate_length);

        SequansController.writeBytes((uint8_t*)certificate_command,
                                     strlen(certificate_command),
                                     true);

        // TODO: figure out which slots to use
        sprintf(command_buffer,
                AT_SETUP_SECURITY_PROFILE_WITH_CERTIFICATES,
                TLS_VERSION,
                ciphers_used,
                USE_CA ? 1 : 0,
                pre_shared_key,
                pre_shared_key_identifier);

        break;

    default:

        Log.error("Unknown authentication method provided! Choose one of the "
                  "MQTT_TLS defines.");

        while (1) {}
        break;
    }

    char buffer[128] = "";

    if (!SequansController.waitForURC("SQNSPCFG",
                                      buffer,
                                      sizeof(buffer),
                                      4000)) {
        Log.infof("Failed to set security profile\r\n");
        return;
    }

    Log.infof("Result: %s\r\n", buffer);

    Log.info("Done!");

    SequansController.end();
    */
}

void loop() {}
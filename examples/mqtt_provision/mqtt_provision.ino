/**
 * @brief This provisioning is for using MQTT with TLS and without the ECC. In
 * order to start the provisioning, just upload this sketch to the board.
 */

#include "led_ctrl.h"
#include "log.h"
#include "sequans_controller.h"

#include <stdint.h>
#include <string.h>

#define MQTT_TLS                         (1)
#define MQTT_TLS_PUBLIC_PRIVATE_KEY_PAIR (2)

#define DEFAULT_CA_SLOT  (1)
#define CUSTOM_CA_SLOT   (16)
#define PUBLIC_KEY_SLOT  (17)
#define PRIVATE_KEY_SLOT (17)

#define AT_SETUP_SECURITY_PROFILE \
    "AT+SQNSPCFG=2,%u,\"%s\",%u,%u,,,\"%s\",\"%s\""
#define AT_SETUP_SECURITY_PROFILE_WITH_CERTIFICATES \
    "AT+SQNSPCFG=2,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\""

#define AT_WRITE_CERTIFICATE "AT+SQNSNVW=\"certificate\",%u,%u"
#define AT_WRITE_PRIVATE_KEY "AT+SQNSNVW=\"privatekey\",%u,%u"

#define NUMBER_OF_CIPHERS      (64)
#define CIPHER_VALUE_LENGTH    (6)
#define CIPHER_TEXT_MAX_LENGTH (50)

#define ASCII_CARRIAGE_RETURN (0xD)
#define ASCII_LINE_FEED       (0xA)
#define ASCII_BACKSPACE       (0x8)
#define ASCII_DELETE          (0x7F)

#define SerialModule Serial3

const char cipher_values[NUMBER_OF_CIPHERS][7] = {
    "0x1301", // TLS_AES_128_GCM_SHA256
    "0x1302", // TLS_AES_256_GCM_SHA384
    "0x1303", // TLS_CHACHA20_POLY1305_SHA256
    "0x1304", // TLS_AES_128_CCM_SHA256
    "0x1305", // TLS_AES_128_CCM_8_SHA256
    "0x000A", // SSL_RSA_WITH_3DES_EDE_CBC_SHA
    "0x002F", // TLS_RSA_WITH_AES_128_CBC_SHA
    "0x0035", // TLS_RSA_WITH_AES_256_CBC_SHA
    "0x0033", // TLS_DHE_RSA_WITH_AES_128_CBC_SHA
    "0x0039", // TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    "0x00AB", // TLS_DHE_PSK_WITH_AES_256_GCM_SHA384
    "0x00AA", // TLS_DHE_PSK_WITH_AES_128_GCM_SHA256
    "0x00A9", // TLS_PSK_WITH_AES_256_GCM_SHA384
    "0x00A8", // TLS_PSK_WITH_AES_128_GCM_SHA256
    "0x00B3", // TLS_DHE_PSK_WITH_AES_256_CBC_SHA384
    "0x00B2", // TLS_DHE_PSK_WITH_AES_128_CBC_SHA256
    "0x00AF", // TLS_PSK_WITH_AES_256_CBC_SHA384
    "0x00AE", // TLS_PSK_WITH_AES_128_CBC_SHA256
    "0x008C", // TLS_PSK_WITH_AES_128_CBC_SHA
    "0x008D", // TLS_PSK_WITH_AES_256_CBC_SHA
    "0xC0A6", // TLS_DHE_PSK_WITH_AES_128_CCM
    "0xC0A7", // TLS_DHE_PSK_WITH_AES_256_CCM
    "0xC0A4", // TLS_PSK_WITH_AES_128_CCM
    "0xC0A5", // TLS_PSK_WITH_AES_256_CCM
    "0xC0A8", // TLS_PSK_WITH_AES_128_CCM_8
    "0xC0A9", // TLS_PSK_WITH_AES_256_CCM_8
    "0xC0A0", // TLS_RSA_WITH_AES_128_CCM_8
    "0xC0A1", // TLS_RSA_WITH_AES_256_CCM_8
    "0xC0AC", // TLS_ECDHE_ECDSA_WITH_AES_128_CCM
    "0xC0AE", // TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8
    "0xC0AF", // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
    "0xC014", // TLS_EDHE_ECDSA_WITH_AES_256_CCM_8
    "0xC013", // TLS_ECCDHE_RSA_WITH_AES_256_CBC_SHA
    "0xC009", // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
    "0xC00A", // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
    "0xC012", // TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
    "0xC008", // TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
    "0x003C", // TLS_RSA_WITH_AES_128_CBC_SHA256
    "0x003D", // TLS_RSA_WITH_AES_256_CBC_SHA256
    "0x0067", // TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
    "0x006B", // TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
    "0x009C", // TLS_RSA_WITH_AES_128_GCM_SHA256
    "0x009D", // TLS_RSA_WITH_AES_256_GCM_SHA384
    "0x009E", // TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
    "0x009F", // TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
    "0xC02F", // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
    "0xC030", // TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
    "0xC02B", // TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
    "0xC02C", // TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
    "0xC027", // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
    "0xC023", // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
    "0xC028", // TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
    "0xC024", // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
    "0xCCA8", // TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
    "0xCCA9", // TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
    "0xCCAA", // TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256
    "0xCC13", // TLS_ECDHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256
    "0xCC14", // TLS_ECDHE_ECDSA_WITH_CHACHA20_OLD_POLY1305_SHA256
    "0xCC15", // TLS_DHE_RSA_WITH_CHACHA20_OLD_POLY1305_SHA256
    "0xC037", // TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
    "0xCCAB", // TLS_PSK_WITH_CHACHA20_POLY1305_SHA256
    "0xCCAC", // TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256
    "0xCCAD", // TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256
    "0x0016", // TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA
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

/**
 * @brief Will read until new line or carriage return. Will clear values from
 * the buffer if the user does a backspace or delete.
 *
 * @param output_buffer Content is placed in this output buffer.
 * @param output_buffer_length The length of the output buffer
 *
 * @return false if the amount of characters read is greater than
 *         @p output_buffer_length.
 */
static bool readUntilNewLine(char* output_buffer,
                             const size_t output_buffer_length) {

    size_t index = 0;

    while (true) {

        while (!SerialModule.available()) {}

        uint8_t input = SerialModule.read();

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
static bool askCloseEndedQuestion(const char* question) {
    while (true) {
        SerialModule.println(question);
        SerialModule.print("Please choose (y/n). Press enter when done: ");

        char input[2] = "";

        if (!readUntilNewLine(input, sizeof(input))) {
            SerialModule.println("\r\nInput too long!");
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
            SerialModule.print("\r\nNot supported choice made: ");
            SerialModule.println(input);
            break;
        }
    }
}

/**
 * @brief Ask a question where a list of alternatives are given. Note that there
 * cannot be a hole in the values ranging from @p min_value from @p max_value.
 *
 * @param min_value Minimum value for the choices presented.
 * @param max_value Maximum value for the choices presented.
 *
 * @return The choice picked.
 */
static uint8_t askNumberedQuestion(const char* question,
                                   const uint8_t min_value,
                                   const uint8_t max_value) {
    while (true) {
        SerialModule.print(question);

        char input[4] = "";

        if (!readUntilNewLine(input, sizeof(input))) {
            SerialModule.println("\r\nInput too long!\r\n");
            continue;
        }

        const uint8_t input_number = strtol(input, NULL, 10);

        if (input_number >= min_value && input_number <= max_value) {
            return input_number;
        } else {
            SerialModule.print("\r\nNot supported choice made: ");
            SerialModule.println(input);
            SerialModule.println();
        }
    }
}

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    256
#define RESPONSE_BUFFER_SIZE 256

void debugBridgeUpdate(void) {
    static uint8_t character;
    static char input_buffer[INPUT_BUFFER_SIZE];
    static uint8_t input_buffer_index = 0;

    if (SerialModule.available() > 0) {
        character = SerialModule.read();

        switch (character) {
        case DEL_CHARACTER:
            if (strlen(input_buffer) > 0) {
                input_buffer[input_buffer_index--] = 0;
            }
            break;

        case ENTER_CHARACTER:
            input_buffer[input_buffer_index]     = '\r';
            input_buffer[input_buffer_index + 1] = '\0';
            SequansController.writeBytes((const uint8_t*)input_buffer,
                                         strlen(input_buffer));

            // Reset buffer
            memset(input_buffer, 0, sizeof(input_buffer));
            input_buffer_index = 0;

            break;

        default:
            input_buffer[input_buffer_index++] = character;
            break;
        }

        SerialModule.print((char)character);
    }

    if (SequansController.isRxReady()) {
        // Send back data from modem to host
        SerialModule.write(SequansController.readByte());
    }
}

/**
 * @brief Asks the user to input a certificate and saves that the NVM for the
 * Sequans modem at the given @p slot.
 *
 * @param message Inital message presented to the user.
 */
static void requestAndSaveCertificate(const char* message, const uint8_t slot) {

    bool got_certificate = false;
    char certificate[4096];
    size_t index = 0;

    while (!got_certificate) {

        memset(certificate, '\0', sizeof(certificate));
        SerialModule.println(message);

        index = 0;

        // Parse line by line until the end is found
        while (true) {

            // A line in x509 is 64 bytes + 1 for null termination
            char line[65] = "";

            if (!readUntilNewLine(line, sizeof(line))) {
                SerialModule.println("\r\nLine input too long!");
                break;
            }

            SerialModule.println();

            const size_t line_length = strlen(line);

            // +1 here for carriage return
            if (index + line_length + 1 >= sizeof(certificate)) {
                SerialModule.print(
                    "\r\nCertificate longer than allowed size of ");
                SerialModule.print(sizeof(certificate));
                SerialModule.println(" bytes.\r\n");

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

                break;
            }

            memcpy(&certificate[index], line, line_length);
            index += line_length;

            if (strstr(line, "-----END CERTIFICATE-----") != NULL) {
                got_certificate = true;
                break;
            } else {
                // Append carriage return as the readString will not inlucde
                // this character, but only to every line except the last
                certificate[index++] = '\n';
            }
        }
    }

    const size_t certificate_length = strlen(certificate);
    char command[48]                = "";

    SerialModule.print("Writing certificate... ");

    SequansController.clearReceiveBuffer();
    sprintf(command, AT_WRITE_CERTIFICATE, slot, certificate_length);
    SequansController.writeBytes((uint8_t*)command, strlen(command), true);

    SequansController.waitForByte('>', 1000);
    SequansController.writeBytes((uint8_t*)certificate,
                                 certificate_length,
                                 true);

    if (SequansController.readResponse() != ResponseResult::OK) {
        SerialModule.println("Error occurred whilst storing certificate");
    } else {
        SerialModule.println("Done");
    }
}

void provisionMqtt() {

    // ------------------------------------------------------------------------
    //              Step 1: Choosing authentication method
    // ------------------------------------------------------------------------

    SerialModule.println();

    const uint8_t method = askNumberedQuestion(
        "Which method do you want to use?\r\n"
        "1: MQTT TLS unauthenticated or with username and password\r\n"
        "2: MQTT TLS with public and private key pair certificates\r\n"
        "Please choose (press enter when done): ",
        1,
        2);

    SerialModule.println("\r\n\r\n");

    // ------------------------------------------------------------------------
    //                      Step 2: Choosing TLS version
    // ------------------------------------------------------------------------

    uint8_t tls_version = askNumberedQuestion(
        "Which TLS version do you want to use?\r\n"
        "1: TLS 1\r\n"
        "2: TLS 1.1\r\n"
        "3: TLS 1.2 (default)\r\n"
        "4: TLS 1.3\r\n"
        "Please choose (press enter when done): ",
        1,
        4);

    // We present the choices for TLS 1 indexed for the user, but the modem uses
    // 0 indexed, so just substract
    tls_version -= 1;

    SerialModule.println("\r\n\r\n");

    // ------------------------------------------------------------------------
    //                      Step 3: Choosing ciphers
    // ------------------------------------------------------------------------

    bool has_chosen_ciphers                = false;
    bool ciphers_chosen[NUMBER_OF_CIPHERS] = {};
    bool chose_automatic_cipher_selection  = true;

    // Each entry takes up 6 characters + delimiter, +1 at end for NULL
    // termination
    char ciphers[(6 + 1) * NUMBER_OF_CIPHERS + 1] = "";
    size_t number_of_ciphers_chosen               = 0;

    char psk[64]          = "";
    char psk_identity[64] = "";

    while (!has_chosen_ciphers) {

        number_of_ciphers_chosen = 0;

        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS; i++) {
            ciphers_chosen[i] = false;
        }

        SerialModule.println(
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

            SerialModule.println(line);
        }

        SerialModule.print("Please choose. Press enter when done (leave blank "
                           "for cipher to be chosen automatically): ");

        char cipher_input[128] = "";

        if (!readUntilNewLine(cipher_input, sizeof(cipher_input))) {
            SerialModule.println("\r\nInput too long!");
            continue;
        }

        has_chosen_ciphers = true;

        // User did not leave the choices blank for automatic cipher
        // selection within the modem, parse them
        if (strlen(cipher_input) > 0) {
            chose_automatic_cipher_selection = false;

            // Split the input to go through each cipher selected
            char* ptr;
            ptr = strtok(cipher_input, ",");

            while (ptr != NULL) {

                // Attempt to convert to number
                uint8_t cipher_index = strtol(ptr, NULL, 10);

                if (cipher_index != 0 && cipher_index <= NUMBER_OF_CIPHERS) {
                    // Choices in the serial input are 1 indexed, so do -1
                    ciphers_chosen[cipher_index - 1] = true;
                } else {
                    SerialModule.print("\r\nUnknown cipher selected: ");
                    SerialModule.println(ptr);
                    has_chosen_ciphers = false;
                    break;
                }

                ptr = strtok(NULL, ",");
            }
        }
    }

    if (chose_automatic_cipher_selection) {
        SerialModule.println("\r\n\r\nCipher will be detected automatically");
    } else {
        SerialModule.println("\r\n\r\nCiphers chosen:");

        // First just record how many ciphers that were chosen
        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS; i++) {
            if (ciphers_chosen[i]) {
                number_of_ciphers_chosen++;
            }
        }

        char line[82]                 = "";
        bool has_chosen_psk_cipher    = false;
        size_t cipher_count           = 0;
        size_t cipher_character_index = 0;

        // Then do the actual processing of the ciphers
        for (uint8_t i = 0; i < NUMBER_OF_CIPHERS; i++) {

            if (ciphers_chosen[i]) {

                // Append the cipher to the string which will be passed with the
                // command to the modem
                memcpy(&ciphers[cipher_character_index],
                       cipher_values[i],
                       CIPHER_VALUE_LENGTH);

                cipher_character_index += CIPHER_VALUE_LENGTH;
                cipher_count++;

                // Add delimiter between ciphers
                if (cipher_count < number_of_ciphers_chosen) {
                    ciphers[cipher_character_index++] = ';';
                }

                // Check if the cipher chosen is a pre shared key one, so that
                // the user has to provide the PSK and the PSK identity
                if (!has_chosen_psk_cipher &&
                    strstr(cipher_text_values[i], "PSK") != NULL) {
                    has_chosen_psk_cipher = true;
                }

                // +1 here since the choices are presented 1 indexed for the
                // user
                sprintf(line, "%2d: %-52s", i + 1, cipher_text_values[i]);
                SerialModule.println(line);
            }
        }

        // --------------------------------------------------------------------
        //              Step 3.5: Check if PSK ciphers are used
        // --------------------------------------------------------------------
        if (has_chosen_psk_cipher) {

            SerialModule.println("\r\nYou have chosen a pre-shared key (PSK) "
                                 "cipher\r\n");

            while (true) {
                SerialModule.print(
                    "Please input the PSK (press enter when done): ");

                if (!readUntilNewLine(psk, sizeof(psk))) {
                    SerialModule.println("\r\nInput too long!");
                } else {
                    break;
                }
            }

            SerialModule.println();

            while (true) {
                SerialModule.print("Please input the PSK identity (press "
                                   "enter when done): ");

                if (!readUntilNewLine(psk_identity, sizeof(psk_identity))) {
                    SerialModule.println("\r\nInput too long!");
                } else {
                    break;
                }
            }
        }
    }

    SerialModule.println("\r\n");

    // ------------------------------------------------------------------------
    //      Step 4: Choose whether to verify the domain using a certificate
    //                          authority (CA)
    // ------------------------------------------------------------------------

    uint8_t ca_index = DEFAULT_CA_SLOT;

    bool verify_ca = askCloseEndedQuestion(
        "\r\nVerify server certificate against "
        "certificate authority (CA)? If you have problems connecting to "
        "the server, you might consider turning this off or loading a CA "
        "which can verify the server.");

    SerialModule.println();

    if (verify_ca) {

        // --------------------------------------------------------------------
        //                      Step 4.5: Custom CA
        // --------------------------------------------------------------------

        bool load_custom_ca = askCloseEndedQuestion(
            "\r\nDo you want to load a custom certificate authority "
            "certificate?");

        if (load_custom_ca) {
            ca_index = CUSTOM_CA_SLOT;

            SerialModule.println();
            requestAndSaveCertificate(
                "Please paste in the CA certifiate and press enter. It should "
                "be on the follwing form:\r\n"
                "-----BEGIN CERTIFICATE-----\r\n"
                "MIIDXTCCAkWgAwIBAgIJAJC1[...]j3tCx2IUXVqRs5mlSbvA==\r\n"
                "-----END CERTIFICATE-----\r\n\r\n",
                ca_index);
        }
    }

    SerialModule.println("\r\n\r\n");

    if (method == MQTT_TLS) {

        // --------------------------------------------------------------------
        //                  Step 5: Write the security profile
        // --------------------------------------------------------------------

        // For regular TLS without public and private key pair we're done now,
        // just need to write the command
        SerialModule.print("Provisioning...");

        char command[strlen(AT_SETUP_SECURITY_PROFILE) + sizeof(ciphers) +
                     sizeof(psk) + sizeof(psk_identity) + 64] = "";

        sprintf(command,
                AT_SETUP_SECURITY_PROFILE,
                tls_version,
                ciphers,
                verify_ca ? 1 : 0,
                ca_index,
                psk,
                psk_identity);

        SequansController.writeBytes((uint8_t*)command, strlen(command), true);

        // Wait for URC confirming the security profile
        if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
            SerialModule.print("Error whilst doing the provisioning");
        } else {
            SerialModule.print(" Done!");
        }

    } else {
        // Load private public key pair
        //

        // --------------------------------------------------------------------
        //             Step 5: Load user's certificate and private key
        // --------------------------------------------------------------------

        SerialModule.println();
        requestAndSaveCertificate(
            "Please paste in the public key certifiate and press enter. It "
            "should be on the follwing form:\r\n"
            "-----BEGIN CERTIFICATE-----\r\n"
            "MIIDXTCCAkWgAwIBAgIJAJC1[...]j3tCx2IUXVqRs5mlSbvA==\r\n"
            "-----END CERTIFICATE-----\r\n\r\n",
            PUBLIC_KEY_SLOT);

        // --------------------------------------------------------------------
        //                  Step 6: Write the security profile
        // --------------------------------------------------------------------

        // TODO: certificate
    }
}

void setup() {
    LedCtrl.begin();
    LedCtrl.startupCycle();

    SerialModule.begin(115200);
    SerialModule.setTimeout(120000);

    SequansController.begin();

    SerialModule.println(
        "\r\n\r\n\r\n=== Provisioning for AVR-IoT Cellular Mini ===");

    provisionMqtt();
}

void loop() { debugBridgeUpdate(); }
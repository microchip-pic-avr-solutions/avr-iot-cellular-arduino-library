#ifndef ECC_CONTROLLER_H
#define ECC_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#define ECC_SERIAL_NUMBER_LENGTH  9
#define ECC_PUBLIC_KEY_LENGTH     64
#define ECC_SIGN_MESSSAGE_LENGTH  32
#define ECC_SIGN_SIGNATURE_LENGTH 64

bool eccControllerInitialize(void);

/**
 * @brief Retrieves the serial number from the ECC.
 *
 * @param serial_number Buffer to place the 9 byte serial number. Has to be
 * pre-allocated to fit ECC_SERIAL_NUMBER_LENGTH bytes.
 *
 * @return true if operation was successfull.
 */
bool eccControllerRetrieveSerialNumber(uint8_t *serial_number);

/**
 * @brief Generates a public/private key pair.
 *
 * @param key_id Where to store the key on the ECC, from slot 0 to 15.
 * @param public_key Destination buffer for the public key, can be NULL if
 * the public key is not needed at the moment. Length: ECC_PUBLIC_KEY_LENGTH
 *
 * @return true if operation was successfull.
 */
bool eccControllerGenerateKeyPair(const uint8_t key_id, uint8_t *public_key);

/**
 * @brief Retrives the public key associated with the private key at the given
 * key id.
 *
 * @param key_id Where the key is stored on the ECC.
 * @param public_key Destination buffer for the public key. Has to be of length
 * ECC_PUBLIC_KEY_LENGTH at minimum.
 *
 * @return true if operation was successfull.
 */
bool eccControllerRetrievePublicKey(const uint8_t key_id, uint8_t *public_key);

/**
 * @brief Signs a message by the private key given by the key id.
 *
 * @param key_id Key id of the private key.
 * @param message Message to sign. Has to be ECC_SIGN_MESSSAGE_LENGTH bytes.
 * @param signature The result from the signature, will be
 * ECC_SIGN_SIGNATURE_LENGTH bytes long.
 *
 * @return true if operation was successfull.
 */
bool eccControllerSignMessage(const uint8_t key_id,
                              const uint8_t *message,
                              uint8_t *signature);

#endif

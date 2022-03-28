#ifndef ECC608_H
#define ECC608_H

#include <Arduino.h>
#include <cryptoauthlib.h>
#include <stddef.h>
#include <stdint.h>

#define SLOT_NUM 8 // ECC slot number used for provisioning data

/*
 * Data header used in ECC slot. The layout is as follows:
 *
 *       +15-----9-8------------------- 0
 *       |  type  |    next (offset)    |
 *       +.-----------------------------        \
 *
 *       It is laid out as 2 bytes in little-endian fashion immediately prior
 *       to the corresponding data blob.
 *           +7--------------------0+
 *       0   |     next[7:0]        |
 *           +7------------1+----0--+
 *       1   |        type  |next[8]|
 *           +--------------+-------+
 *       2   |        data          |
 *       ... |                      |
 *           +----------------------+
 *
 * An empty slot (type == 0) ends the list, unless the whole slot is used.
 */
struct DataHeader {
    uint16_t next : 9; // Offset of next header, up to 512 bytes (slot 8 is 416)
    uint16_t type : 7; // Type of current item, see provision_data.h
};

enum ecc_data_types {
    EMPTY = 0,
    AWS_THINGNAME = 1,
    AWS_ENDPOINT = 2,
    AZURE_ID_SCOPE = 3,
    AZURE_IOT_HUB_NAME = 4,
    AZURE_DEVICE_ID = 5,
    GOOGLE_PROJECT_ID = 6,
    GOOGLE_PROJECT_REGION = 7,
    GOOGLE_REGISTRY_ID = 8,
    GOOGLE_DEVICE_ID = 9,
    NUM_TYPES // Placeholder, keep this last
};

class ECC608Class {

  private:
    /**
     * @brief Hide constructor in order to enforce a single instance of the
     * class.
     */
    ECC608Class(){};

    /*
     * @brief Extract item with given type from ECC slot.
     *
     * This code traverses the list until matching type is found,
     * assuming there is only one item of each type in the slot.
     * Notice: A \0 terminator is added to the returned item for
     *         easier string processing. This is not included in returned
     *         length, but there must be space for it in the buffer.
     *
     * @param type Type of requested item
     * @param buffer Buffer to store item in
     * @param length Pointer to length of buffer (== max length of returned
     * data), set to actual item length on return
     *
     * @return Error code: 0 => OK, 1-0xff => cryptoauth error code, >= 0x100:
     * see ecc_provision.h
     */
    uint8_t readProvisionItem(enum ecc_data_types type,
                              uint8_t *buffer,
                              uint8_t *length);

  public:
    /**
     * @brief Singleton instance.
     */
    static ECC608Class &instance(void) {
        static ECC608Class instance;
        return instance;
    }

    /*
     * Error codes returned by this library. LSB is reserved for
     * cryptoauthlib error codes.
     */
    enum ecc_error_code {
        ERR_OK = 0,
        ERR_NOTFOUND, // Requested item not found
        ERR_TOOLARGE, // Item too large to fit buffer
    };

    uint8_t getEndpoint(uint8_t *endpoint, uint8_t *length);

    uint8_t getThingName(uint8_t *thingName, uint8_t *length);

    uint8_t begin();
};

extern ECC608Class ECC608;

#endif

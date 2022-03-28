#include "ecc608.h"
#include <Arduino.h>

#define SLOT_NUM 8 // ECC slot number used for provisioning data

static bool initialized = false;

uint8_t ECC608Class::getEndpoint(uint8_t *endpoint, uint8_t *length) {
    return this->readProvisionItem(AWS_ENDPOINT, endpoint, length);
}

uint8_t ECC608Class::getThingName(uint8_t *thingName, uint8_t *length) {
    return this->readProvisionItem(AWS_THINGNAME, thingName, length);
}

uint8_t ECC608Class::begin() {
    if (initialized) {
        return 0;
    } else {
        initialized = true;
    }

    static ATCAIfaceCfg ECCConfig;

    ECCConfig = {ATCA_I2C_IFACE,
                 ATECC608B,
                 {
                     0x58,  // 7 bit address of ECC
                     2,     // Bus number
                     100000 // Baud rate
                 },
                 1560,
                 20,
                 NULL};

    return atcab_init(&ECCConfig);
}

uint8_t ECC608Class::readProvisionItem(enum ecc_data_types type,
                                       uint8_t *buffer,
                                       uint8_t *length) {
    uint8_t atca_status = ATCA_SUCCESS;
    // Use union for data header to avoid pointer casting
    union {
        struct DataHeader header;
        uint8_t bytes[sizeof(struct DataHeader)];
    } h;
    size_t slot_size;
    uint16_t offset = 0;

    // Determine slot size
    if ((atca_status = atcab_get_zone_size(
             ATCA_ZONE_DATA, SLOT_NUM, &slot_size)) != ATCA_SUCCESS) {
        *length = 0;
        return atca_status;
    }

    // Traverse items in slot, return matching item in buffer, actual length in
    // *length
    do {
        // Read data header from ECC slot
        if ((atca_status = atcab_read_bytes_zone(
                 ATCA_ZONE_DATA, SLOT_NUM, offset, h.bytes, sizeof(h))) !=
            ATCA_SUCCESS) {
            *length = 0;
            return atca_status;
        }
        if (h.header.type == type) {
            uint16_t size = h.header.next - offset - sizeof(h);
            if (size + 1 <=
                *length) { // make sure there is room for \0 terminator
                // Read the actual data item
                if ((atca_status = atcab_read_bytes_zone(ATCA_ZONE_DATA,
                                                         SLOT_NUM,
                                                         offset + sizeof(h),
                                                         buffer,
                                                         size)) !=
                    ATCA_SUCCESS) {
                    *length = 0;
                    return atca_status;
                }
                *length = size;
                buffer[size] = '\0';
                return this->ERR_OK;
            } else {
                *length = 0;
                return this->ERR_TOOLARGE;
            }
        }
        offset = h.header.next;
    } while (h.header.type != EMPTY && offset + sizeof(h) <= slot_size);

    *length = 0;
    return this->ERR_NOTFOUND;
}

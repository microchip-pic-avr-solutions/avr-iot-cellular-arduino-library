#include "ecc608.h"

#include "cryptoauthlib/app/tng/tng_atcacert_client.h"

#define SLOT_NUM 8 // ECC slot number used for provisioning data

/*
 * Data header used in ECC slot. The layout is as follows:
 *
 *       +15-----9-8------------------- 0
 *       |  type  |    next (offset)    |
 *       +.-----------------------------+
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

/**
 * @brief Convenience union for reading/writing header data.
 */
typedef union {
    struct DataHeader header;
    uint8_t bytes[sizeof(struct DataHeader)];
} HeaderUnion;

ECC608Class ECC608 = ECC608Class::instance();

ATCA_STATUS ECC608Class::begin() {
    if (initialized) {
        return ATCA_SUCCESS;
    } else {
        initialized = true;
    }

    static ATCAIfaceCfg ECCConfig = {ATCA_I2C_IFACE,
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

ATCA_STATUS ECC608Class::readProvisionItem(const enum ecc_data_types type,
                                           uint8_t* buffer,
                                           size_t* size) {

    ATCA_STATUS atca_status = ATCA_SUCCESS;
    HeaderUnion header_union;

    size_t slot_size;
    uint16_t offset = 0;

    // Determine slot size
    if ((atca_status = atcab_get_zone_size(ATCA_ZONE_DATA,
                                           SLOT_NUM,
                                           &slot_size)) != ATCA_SUCCESS) {
        *size = 0;
        return atca_status;
    }

    // Traverse items in slot, return matching item in buffer, actual length in
    // *size
    do {
        // Read data header from ECC slot
        if ((atca_status = atcab_read_bytes_zone(ATCA_ZONE_DATA,
                                                 SLOT_NUM,
                                                 offset,
                                                 header_union.bytes,
                                                 sizeof(HeaderUnion))) !=
            ATCA_SUCCESS) {
            *size = 0;
            return atca_status;
        }

        if (header_union.header.type == type) {
            size_t data_size = header_union.header.next - offset -
                               sizeof(HeaderUnion);

            // Make sure there is room for \0 terminator
            if (data_size + 1 <= *size) {

                // Read the actual data item
                if ((atca_status = atcab_read_bytes_zone(
                         ATCA_ZONE_DATA,
                         SLOT_NUM,
                         offset + sizeof(HeaderUnion),
                         buffer,
                         data_size)) != ATCA_SUCCESS) {
                    *size = 0;
                    return atca_status;
                }

                *size             = data_size;
                buffer[data_size] = '\0';

                return ATCA_SUCCESS;
            } else {

                *size = 0;
                return ATCA_SMALL_BUFFER;
            }
        }
        offset = header_union.header.next;

    } while (header_union.header.type != EMPTY &&
             offset + sizeof(HeaderUnion) <= slot_size);

    *size = 0;

    return ATCA_INVALID_ID;
}

ATCA_STATUS
ECC608Class::writeProvisionData(const size_t number_of_provision_items,
                                const enum ecc_data_types* types,
                                const uint8_t** data,
                                const size_t* data_sizes) {

    ATCA_STATUS atca_status = ATCA_SUCCESS;

    size_t slot_size = 0;
    if ((atca_status = atcab_get_zone_size(ATCA_ZONE_DATA,
                                           SLOT_NUM,
                                           &slot_size)) != ATCA_SUCCESS) {
        return atca_status;
    }

    // Clear the data in the slot, this is just in case there is an edge case
    // where the content written here aligns with the previous items and we can
    // have duplicates of entries. We keep this in a nested scope here to free
    // the buffer from the stack.
    {
        const uint8_t buffer[slot_size] = {0x00};

        if ((atca_status = atcab_write_bytes_zone(ATCA_ZONE_DATA,
                                                  SLOT_NUM,
                                                  0,
                                                  buffer,
                                                  slot_size)) != ATCA_SUCCESS) {
            return atca_status;
        }
    }

    // First we calculate the total size including headers
    size_t payload_size = 0;

    for (size_t i = 0; i < number_of_provision_items; i++) {
        payload_size += sizeof(HeaderUnion) + data_sizes[i];
    }

    // We have to make sure that the payload is a multiple of 32-bytes for the
    // write, see the documentation for atcab_write_byte_zones for more
    // information
    payload_size = (payload_size / 32 + ((payload_size % 32 != 0) ? 1 : 0)) *
                   32;

    if (payload_size > slot_size) {
        return ATCA_INVALID_SIZE;
    }

    // Now we can build the payload data which includes the headers and the
    // actual data
    uint8_t payload[payload_size];
    size_t offset = 0;

    for (size_t i = 0; i < number_of_provision_items; i++) {
        HeaderUnion header_union;

        header_union.header.type = types[i];
        header_union.header.next = offset + sizeof(HeaderUnion) + data_sizes[i];

        memcpy(payload + offset, header_union.bytes, sizeof(HeaderUnion));
        memcpy(payload + offset + sizeof(HeaderUnion), data[i], data_sizes[i]);

        offset += sizeof(HeaderUnion) + data_sizes[i];
    }

    if ((atca_status = atcab_write_bytes_zone(ATCA_ZONE_DATA,
                                              SLOT_NUM,
                                              0,
                                              payload,
                                              payload_size)) != ATCA_SUCCESS) {
        return atca_status;
    }

    // Verify that the data was written correctly
    for (size_t i = 0; i < number_of_provision_items; i++) {
        uint8_t buffer[data_sizes[i] + 1];
        size_t size = data_sizes[i] + 1;

        if ((atca_status = readProvisionItem(types[i], buffer, &size)) !=
            ATCA_SUCCESS) {
            return atca_status;
        }

        if (size != data_sizes[i] || memcmp(buffer, data[i], size) != 0) {
            return ATCA_ASSERT_FAILURE;
        }
    }

    return ATCA_SUCCESS;
}

ATCA_STATUS ECC608Class::getThingName(uint8_t* thing_name, uint8_t* size) {
    // Need this to bridge the compatibility between using size_t and uint8_t.
    // This function will be deprecated later.
    size_t thing_name_size = *size;
    const ATCA_STATUS result =
        readProvisionItem(AWS_THINGNAME, thing_name, &thing_name_size);
    *size = thing_name_size;

    return result;
}

ATCA_STATUS ECC608Class::getEndpoint(uint8_t* endpoint, uint8_t* size) {
    // Need this to bridge the compatibility between using size_t and uint8_t.
    // This function will be deprecated later.
    size_t endpoint_size = *size;
    const ATCA_STATUS result =
        readProvisionItem(AWS_ENDPOINT, endpoint, &endpoint_size);
    *size = endpoint_size;

    return result;
}

int ECC608Class::getRootCertificateSize(size_t* size) {
    return tng_atcacert_root_cert_size(size);
}

int ECC608Class::getRootCertificate(uint8_t* certificate, size_t* size) {
    return tng_atcacert_root_cert(certificate, size);
}

int ECC608Class::getSignerCertificateSize(size_t* size) {
    return tng_atcacert_max_signer_cert_size(size);
}

int ECC608Class::getSignerCertificate(uint8_t* certificate, size_t* size) {

    const ATCA_STATUS status = atcab_wakeup();

    if (status != ATCA_SUCCESS) {
        return -99;
    }

    return tng_atcacert_read_signer_cert(certificate, size);
}

int ECC608Class::getDeviceCertificateSize(size_t* size) {
    return tng_atcacert_max_device_cert_size(size);
}

int ECC608Class::getDeviceCertificate(uint8_t* certificate, size_t* size) {
    return tng_atcacert_read_device_cert(certificate, size, NULL);
}

size_t ECC608Class::calculateBase64EncodedCertificateSize(
    const size_t certificate_size) {
    // For Base64 encoding, we need ceil(4 * (n / 3)) character to represent n
    // bytes.
    size_t base64_size = (certificate_size / 3 + (certificate_size % 3 != 0)) *
                         4;

    // Make space for newlines. atcab_b64rules_default[3] specifes the amount of
    // characters per line which is used in the default configuration when
    // calling atcab_base64encode, so we need two extra characters for carriage
    // return and newline for each line.
    base64_size += (base64_size / atcab_b64rules_default[3]) * 2;

    // Make space for null termination.
    base64_size += 1;

    return base64_size;
}

ATCA_STATUS
ECC608Class::base64EncodeCertificate(const uint8_t* certificate,
                                     const size_t certificate_size,
                                     char* base64_encoded_certificate,
                                     size_t* base64_encoded_certificate_size) {
    return atcab_base64encode(certificate,
                              certificate_size,
                              base64_encoded_certificate,
                              base64_encoded_certificate_size);
}
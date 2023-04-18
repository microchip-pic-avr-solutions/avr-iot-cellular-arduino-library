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

ATCA_STATUS ECC608Class::readProvisionItem(const enum EccDataType type,
                                           uint8_t* buffer,
                                           size_t* size) {

    ATCA_STATUS atca_status = ATCA_SUCCESS;

    // Use union for data header to avoid pointer casting
    union {
        struct DataHeader header;
        uint8_t bytes[sizeof(struct DataHeader)];
    } h;

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
                                                 h.bytes,
                                                 sizeof(h))) != ATCA_SUCCESS) {
            *size = 0;
            return atca_status;
        }

        if (h.header.type == type) {
            size_t data_size = h.header.next - offset - sizeof(h);

            // Make sure there is room for \0 terminator
            if (data_size + 1 <= *size) {

                // Read the actual data item
                if ((atca_status = atcab_read_bytes_zone(ATCA_ZONE_DATA,
                                                         SLOT_NUM,
                                                         offset + sizeof(h),
                                                         buffer,
                                                         data_size)) !=
                    ATCA_SUCCESS) {
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
        offset = h.header.next;

    } while (h.header.type != EMPTY && offset + sizeof(h) <= slot_size);

    *size = 0;

    return ATCA_INVALID_ID;
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
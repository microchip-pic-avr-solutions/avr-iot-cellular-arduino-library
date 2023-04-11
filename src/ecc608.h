#ifndef ECC608_H
#define ECC608_H

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

#include "atcacert/atcacert.h"
#include "cryptoauthlib.h"

class ECC608Class {

  private:
    /**
     * @brief Set when #begin() is called.
     */
    bool initialized = false;

    /**
     * @brief Hide constructor in order to enforce a single instance of the
     * class.
     */
    ECC608Class(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static ECC608Class& instance(void) {
        static ECC608Class instance;
        return instance;
    }

    /**
     * @brief Initializes the ECC.
     *
     * @return The enumerations of #ATCA_STATUS. #ATCA_SUCCESS on success.
     */
    ATCA_STATUS begin();

    /**
     * @brief Retrieves the AWS endpoint stored in the ECC.
     *
     * @param endpoint [out] Buffer to place the endpoint.
     * @param size [in, out] Size of buffer. Will be overwritten with the
     * endpoint length.
     *
     * @return The enumerations of cryptoauthlib's #ATCA_STATUS. #ATCA_SUCCESS
     * on success.
     */
    ATCA_STATUS getEndpoint(uint8_t* endpoint, size_t* size);

    /**
     * @brief Retrieves the thing name stored in the ECC.
     *
     * @param thing_name [out] Buffer to place the thing name.
     * @param size [in, out] size of buffer. Will be overwritten with the
     * thing name length.
     *
     * @return The enumerations of cryptoauthlib's #ATCA_STATUS. #ATCA_SUCCESS
     * on success.
     */
    ATCA_STATUS getThingName(uint8_t* thing_name, size_t* size);

    /**
     * @brief Get the size of the root certificate (not base64 encoded. The size
     * is in raw bytes).
     *
     * @param size [out] The size will be placed in this pointer.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getRootCertificateSize(size_t* size);

    /**
     * @brief Retrieve the root certificate. Note that the certificate format is
     * not base64 encoded.
     *
     * @param certificate [out] Buffer where the certificate will be placed.
     * @param size [in, out] Size of buffer as input. Will be overwritten with
     * the size of the certificate.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getRootCertificate(uint8_t* certificate, size_t* size);

    /**
     * @brief Get the size of the signer certificate (not base64 encoded. The
     * size is in raw bytes).
     *
     * @param size [out] The size will be placed in this pointer.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getSignerCertificateSize(size_t* size);

    /**
     * @brief Retrieve the signer certificate. Note that the certificate format
     * is not base64 encoded.
     *
     * @param certificate [out] Buffer where the certificate will be placed.
     * @param size [in, out] Size of buffer as input. Will be overwritten with
     * the size of the certificate.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getSignerCertificate(uint8_t* certificate, size_t* size);

    /**
     * @brief Get the size of the device certificate (not base64 encoded. The
     * size is in raw bytes).
     *
     * @param size [out] The size will be placed in this pointer.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getDeviceCertificateSize(size_t* size);

    /**
     * @brief Retrieve the device certificate. Note that the certificate format
     * is not base64 encoded.
     *
     * @param certificate [out] Buffer where the certificate will be placed.
     * @param size [in, out] Size of buffer as input. Will be overwritten with
     * the size of the certificate.
     *
     * @return The #ATCACERT_x defines. @see atcacert.h.
     */
    int getDeviceCertificate(uint8_t* certificate, size_t* size);

    /**
     * @brief Calculates the buffer size needed for encoding a certificate in
     * raw bytes to base64.
     *
     * @param certificate_size [in] The certificate size in raw bytes.
     *
     * @return The size needed for base64 encoding.
     */
    size_t calculateBase64EncodedCertificateSize(const size_t certificate_size);

    /**
     * @brief Base64 encodes a certificate in raw byte format.
     *
     * @param certificate [in] The certificate in raw bytes.
     * @param certificate_size [in] Size of the certificate in raw bytes.
     * @param base64_encoded_certificate [out] The base64 encoded certificate
     * will be placed in this buffer.
     * @param base64_encoded_certificate_size [in, out] Size of @p
     * base64_encoded_certificate, will be overwritten with the final size of
     * the encoded certificate.
     *
     * @return The enumerations of cryptoauthlib's #ATCA_STATUS. #ATCA_SUCCESS
     * on success.
     */
    ATCA_STATUS
    base64EncodeCertificate(const uint8_t* certificate,
                            const size_t certificate_size,
                            char* base64_encoded_certificate,
                            size_t* base64_encoded_certificate_size);
};

extern ECC608Class ECC608;

#endif

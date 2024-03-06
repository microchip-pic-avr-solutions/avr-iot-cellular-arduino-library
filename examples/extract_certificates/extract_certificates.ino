#include <Arduino.h>

#include <ecc608.h>
#include <log.h>

static void printCertificate(const uint8_t* certificate, const size_t size) {

    size_t buffer_size = ECC608.calculateBase64EncodedCertificateSize(size);

    char buffer[buffer_size];

    ATCA_STATUS status =
        ECC608.base64EncodeCertificate(certificate, size, buffer, &buffer_size);

    if (status != ATCA_SUCCESS) {
        Log.errorf(F("Failed to encode into base64: %x\r\n"), status);
        return;
    }

    Log.rawf(
        F("-----BEGIN CERTIFICATE-----\r\n%s\r\n-----END CERTIFICATE-----\r\n"),
        buffer);
}

void setup() {
    Log.begin(115200);

    ATCA_STATUS atca_status = ECC608.begin();

    if (atca_status != ATCA_SUCCESS) {
        Log.errorf(F("Failed to initialize ECC608, status code: %d\r\n"),
                   atca_status);
        return;
    }

    Log.info(F("Initialized ECC\r\n"));

    // Extract the max size of the certificates first

    size_t max_root_certificate_size = 0, max_signer_certificate_size = 0,
           max_device_certificate_size = 0;

    int atca_cert_status = ATCACERT_E_SUCCESS;

    if ((atca_cert_status = ECC608.getRootCertificateSize(
             &max_root_certificate_size)) != ATCACERT_E_SUCCESS) {

        Log.errorf(F("Failed to get root certificate's max size, status code: "
                     "%d\r\n"),
                   atca_cert_status);
        return;
    }

    if ((atca_cert_status = ECC608.getSignerCertificateSize(
             &max_signer_certificate_size)) != ATCACERT_E_SUCCESS) {

        Log.errorf(F("Failed to get signer certificate's max size, status "
                     "code: %d\r\n"),
                   atca_cert_status);
        return;
    }

    if ((atca_cert_status = ECC608.getDeviceCertificateSize(
             &max_device_certificate_size)) != ATCACERT_E_SUCCESS) {

        Log.errorf(F("Failed to get device certificate's max size, status "
                     "code: %d\r\n"),
                   atca_cert_status);
        return;
    }

    // We reuse the buffer for the certificates, so have to find the max
    // size of them so we have enough space for the biggest certificate
    const size_t certificate_buffer_size = max(
        max(max_root_certificate_size, max_signer_certificate_size),
        max_device_certificate_size);

    uint8_t certificate_buffer[certificate_buffer_size];

    // --- Root certificate ---

    size_t root_certificate_size = certificate_buffer_size;

    Log.raw(F("\r\n\r\n"));

    if ((atca_cert_status = ECC608.getRootCertificate(
             certificate_buffer,
             &root_certificate_size)) != ATCACERT_E_SUCCESS) {

        Log.errorf(F("Failed to get root certificate, status code: "
                     "%d\r\n"),
                   atca_cert_status);
        return;
    } else {

        Log.info(F("Printing root certificate...\r\n"));
        printCertificate(certificate_buffer, root_certificate_size);
    }

    // --- Signer certificate ---

    size_t signer_certificate_size = max_signer_certificate_size;

    Log.raw("\r\n\r\n");

    if ((atca_cert_status = ECC608.getSignerCertificate(
             certificate_buffer,
             &signer_certificate_size)) != ATCACERT_E_SUCCESS) {

        Log.errorf(F("Failed to get signer certificate, status code: "
                     "%d\r\n"),
                   atca_cert_status);
        return;
    } else {

        Log.info(F("Printing signer certificate...\r\n"));
        printCertificate(certificate_buffer, signer_certificate_size);
    }

    // --- Device certificate ---

    size_t device_certificate_size = max_device_certificate_size;

    Log.raw(F("\r\n\r\n"));

    if ((atca_cert_status = ECC608.getDeviceCertificate(
             certificate_buffer,
             &device_certificate_size)) != ATCACERT_E_SUCCESS) {

        Log.errorf(F("Failed to get device certificate, status code: "
                     "0x%X\r\n"),
                   atca_cert_status);
        return;
    } else {

        Log.info(F("Printing device certificate...\r\n"));
        printCertificate(certificate_buffer, device_certificate_size);
    }
}

void loop() {}

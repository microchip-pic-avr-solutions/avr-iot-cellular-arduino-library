/**
 * \file
 * \brief Functions required to work with PEM encoded data related to X.509
 * certificates.
 *
 * \copyright (c) 2015-2020 Microchip Technology Inc. and its subsidiaries.
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip software
 * and any derivatives exclusively with Microchip products. It is your
 * responsibility to comply with third party license terms applicable to your
 * use of third party software (including open source software) that may
 * accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT,
 * SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE
 * OF ANY KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF
 * MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE
 * FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL
 * LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED
 * THE AMOUNT OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR
 * THIS SOFTWARE.
 */

#include <string.h>

#include "cryptoauthlib/lib/atcacert/atcacert.h"
#include "cryptoauthlib/lib/atcacert/atcacert_pem.h"
#include "cryptoauthlib/lib/atca_helpers.h"

#if ATCACERT_COMPCERT_EN

int atcacert_encode_pem(const uint8_t* der,
                        size_t         der_size,
                        char*          pem,
                        size_t*        pem_size,
                        const char*    header,
                        const char*    footer)
{
    ATCA_STATUS status;
    size_t max_pem_size;
    size_t header_size;
    size_t footer_size;
    size_t b64_size;
    size_t pem_index = 0;

    if (der == NULL || pem == NULL || pem_size == NULL || header == NULL || footer == NULL)
    {
        return ATCACERT_E_BAD_PARAMS;
    }
    max_pem_size = *pem_size;
    *pem_size = 0; // Default to 0

    // Add header
    header_size = strlen(header);
    if (pem_index + header_size + 2 > max_pem_size)
    {
        return ATCACERT_E_BUFFER_TOO_SMALL;
    }
    memcpy(&pem[pem_index], header, header_size);
    pem_index += header_size;
    memcpy(&pem[pem_index], "\r\n", 2);
    pem_index += 2;

    // Add base64 encoded DER data with \r\n every 64 characters
    b64_size = max_pem_size - pem_index;
    status = atcab_base64encode(der, der_size, &pem[pem_index], &b64_size);
    if (status != ATCACERT_E_SUCCESS)
    {
        if (status == ATCA_SMALL_BUFFER)
        {
            status = (ATCA_STATUS)ATCACERT_E_BUFFER_TOO_SMALL;
        }
        return status;
    }
    pem_index += b64_size;

    // Add \r\n after data
    footer_size = strlen(footer);
    if (pem_index + 2 + footer_size + 2 + 1 > max_pem_size)
    {
        return ATCACERT_E_BUFFER_TOO_SMALL;
    }
    memcpy(&pem[pem_index], "\r\n", 2);
    pem_index += 2;

    // Add footer
    memcpy(&pem[pem_index], footer, footer_size);
    pem_index += footer_size;
    memcpy(&pem[pem_index], "\r\n", 2);
    pem_index += 2;

    pem[pem_index] = 0; // Terminating null, not included in size

    // Set output size
    *pem_size = pem_index;

    return ATCACERT_E_SUCCESS;
}

int atcacert_decode_pem(const char* pem,
                        size_t      pem_size,
                        uint8_t*    der,
                        size_t*     der_size,
                        const char* header,
                        const char* footer)
{
    ATCA_STATUS status;
    const char* header_pos = NULL;
    const char* data_pos = NULL;
    const char* footer_pos = NULL;

    (void)pem_size;

    if (pem == NULL || der == NULL || der_size == NULL || header == NULL || footer == NULL)
    {
        return ATCACERT_E_BAD_PARAMS;
    }

    // Find the position of the header
    header_pos = strstr(pem, header);
    if (header_pos == NULL)
    {
        return ATCACERT_E_DECODING_ERROR; // Couldn't find header
    }

    // Data should be right after the header. Not accounting for new lines as
    // the base64 decode should skip over those.
    data_pos = header_pos + strlen(header);

    // Find footer
    footer_pos = strstr(pem, footer);
    if (footer_pos == NULL || footer_pos < data_pos)
    {
        // Couldn't find footer or found it before the data
        return ATCACERT_E_DECODING_ERROR;
    }

    // Decode data
    status = atcab_base64decode(data_pos, (size_t)(footer_pos - data_pos), der, der_size);
    if (status != ATCA_SUCCESS)
    {
        if (status == ATCA_SMALL_BUFFER)
        {
            status = (ATCA_STATUS)ATCACERT_E_BUFFER_TOO_SMALL;
        }
        return status;
    }

    return ATCACERT_E_SUCCESS;
}

int atcacert_encode_pem_cert(const uint8_t* der_cert, size_t der_cert_size, char* pem_cert, size_t* pem_cert_size)
{
    return atcacert_encode_pem(
        der_cert,
        der_cert_size,
        pem_cert,
        pem_cert_size,
        PEM_CERT_BEGIN,
        PEM_CERT_END);
}

int atcacert_encode_pem_csr(const uint8_t* der_csr, size_t der_csr_size, char* pem_csr, size_t* pem_csr_size)
{
    return atcacert_encode_pem(
        der_csr,
        der_csr_size,
        pem_csr,
        pem_csr_size,
        PEM_CSR_BEGIN,
        PEM_CSR_END);
}

int atcacert_decode_pem_cert(const char* pem_cert, size_t pem_cert_size, uint8_t* der_cert, size_t* der_cert_size)
{
    return atcacert_decode_pem(
        pem_cert,
        pem_cert_size,
        der_cert,
        der_cert_size,
        PEM_CERT_BEGIN,
        PEM_CERT_END);
}

int atcacert_decode_pem_csr(const char* pem_csr, size_t pem_csr_size, uint8_t* der_csr, size_t* der_csr_size)
{
    return atcacert_decode_pem(
        pem_csr,
        pem_csr_size,
        der_csr,
        der_csr_size,
        PEM_CSR_BEGIN,
        PEM_CSR_END);
}

#endif
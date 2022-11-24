#include "http_client.h"
#include "log.h"
#include "security_profile.h"
#include "sequans_controller.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// We only use profile 0 to keep things simple we also stick with spId 3
// which we dedicate to HTTPS
#define HTTP_CONFIGURE "AT+SQNHTTPCFG=0,\"%s\",%u,0,\"\",\"\",%u,120,1,3"

// Command without any data in it (with quotation marks): 36 bytes
// Max length of doman name: 127 bytes
// Max length of port number: 5 bytes (0-65535)
// TLS enabled: 1 byte
// Termination: 1 byte
// This results in 36 + 127 + 5 + 1 + 1 = 170
#define HTTP_CONFIGURE_SIZE 170

#define HTTPS_SECURITY_PROFILE_NUMBER 3

#define HTTP_SEND    "AT+SQNHTTPSND=0,%u,\"%s\",%lu,\"%s\",\"%s\""
#define HTTP_RECEIVE "AT+SQNHTTPRCV=0,%lu"
#define HTTP_QUERY   "AT+SQNHTTPQRY=0,%u,\"%s\",\"%s\""

#define HTTP_RING_URC "SQNHTTPRING"

#define HTTP_POST_METHOD   0
#define HTTP_PUT_METHOD    1
#define HTTP_GET_METHOD    0
#define HTTP_HEAD_METHOD   1
#define HTTP_DELETE_METHOD 2

// Content type specifiers for POST requests for the AT+SQNHTTPSND command
#define HTTP_CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED "0"
#define HTTP_CONTENT_TYPE_TEXT_PLAIN                        "1"
#define HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM          "2"
#define HTTP_CONTENT_TYPE_APPLICATION_MULTIPART_FORM_DATA   "3"
#define HTTP_CONTENT_TYPE_APPLICATION_APPLICATION_JSON      "4"

#define HTTP_RECEIVE_LENGTH          32
#define HTTP_RECEIVE_START_CHARACTER '<'
#define HTTP_SEND_START_CHARACTER    '>'

#define HTTP_RESPONSE_MAX_LENGTH         84
#define HTTP_RESPONSE_STATUS_CODE_INDEX  1
#define HTTP_RESPONSE_STATUS_CODE_LENGTH 3
#define HTTP_RESPONSE_DATA_SIZE_INDEX    3
#define HTTP_RESPONSE_DATA_SIZE_LENGTH   16

// These are limitations from the Sequans module, so the range of bytes we can
// receive with one call to the read body AT command has to be between these
// values. One thus has to call the function multiple times if the data size is
// greater than the max size
#define HTTP_BODY_BUFFER_MIN_SIZE 64
#define HTTP_BODY_BUFFER_MAX_SIZE 1500

#define HTTP_TIMEOUT 20000

HttpClientClass HttpClient = HttpClientClass::instance();

/**
 * @brief Generic method for sending data via HTTP, either with POST or PUT.
 * Issues an AT command to the LTE modem.
 *
 * @param endpoint Destination of payload, part after host name in URL.
 * @param data Payload to send.
 * @param data_length Length of payload.
 * @param method POST(0) or PUT(1).
 * @param header Optional header.
 * @param header_length Length of header.
 */
static HttpResponse sendData(const char* endpoint,
                             const uint8_t* data,
                             const uint32_t data_length,
                             const uint8_t method,
                             const uint8_t* header        = NULL,
                             const uint32_t header_length = 0,
                             const char* content_type     = "") {

    // The modem could hang if several HTTP requests are done quickly after each
    // other, this alleviates this
    SequansController.writeCommand("AT");

    HttpResponse http_response = {0, 0};

    // Setup and transmit SEND command before sending the data
    const uint32_t digits_in_data_length = trunc(log10(data_length)) + 1;

    const uint32_t command_length = strlen(HTTP_SEND) + strlen(endpoint) +
                                    digits_in_data_length + header_length;

    // Append +1 for NULL termination
    char command[command_length + 1];

    // Append +1 for NULL termination
    snprintf(command,
             command_length + 1,
             HTTP_SEND,
             method,
             endpoint,
             (unsigned long)data_length,
             content_type,
             header == NULL ? "" : (const char*)header);

    SequansController.writeBytes((uint8_t*)command, command_length, true);

    // Only send the data payload if there is any
    if (data_length > 0) {

        if (!SequansController.waitForByte(HTTP_SEND_START_CHARACTER,
                                           HTTP_TIMEOUT)) {
            Log.error("Timed out whilst waiting on delivering the HTTP "
                      "payload. Is the "
                      "server online?");
            return http_response;
        }

        // Wait some before delivering the payload. The modem might hang if we
        // deliver it too quickly
        delay(100);

        // Now we deliver the payload
        SequansController.writeBytes(data, data_length);
    }

    char http_response_buffer[HTTP_RESPONSE_MAX_LENGTH]                = "";
    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";
    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH]              = "";

    // Now we wait for the URC
    if (!SequansController.waitForURC(HTTP_RING_URC,
                                      http_response_buffer,
                                      sizeof(http_response_buffer))) {
        Log.warn("Did not get HTTP response before timeout\r\n");
        return http_response;
    }

    // We pass in NULL as the start character here as the URC data will only
    // contain the payload, not the URC identifier
    bool got_response_code = SequansController.extractValueFromCommandResponse(
        http_response_buffer,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1,
        (char)NULL);

    bool got_data_size = SequansController.extractValueFromCommandResponse(
        http_response_buffer,
        HTTP_RESPONSE_DATA_SIZE_INDEX,
        data_size_buffer,
        HTTP_RESPONSE_DATA_SIZE_LENGTH,
        (char)NULL);

    if (got_response_code) {
        http_response.status_code = atoi(http_status_code_buffer);
    }

    if (got_data_size) {
        http_response.data_size = atoi(data_size_buffer);
    }

    return http_response;
}

/**
 * @brief Generic method for retrieving data via HTTP, either with HEAD, GET or
 * DELETE.
 *
 * @param endpoint Destination of retrieve, part after host name in URL.
 * @param method GET(0), HEAD(1) or DELETE(2).
 * @param header Optional header.
 * @param header_length Length of header.
 */
static HttpResponse queryData(const char* endpoint,
                              const uint8_t method,
                              const uint8_t* header,
                              const uint32_t header_length) {

    // The modem could hang if several HTTP requests are done quickly after each
    // other, this alleviates this
    SequansController.writeCommand("AT");

    HttpResponse http_response = {0, 0};

    // Set up and send the query
    const uint32_t command_length = strlen(HTTP_QUERY) + strlen(endpoint) +
                                    header_length;

    // Append +1 for NULL termination
    char command[command_length + 1];

    snprintf(command,
             command_length + 1,
             HTTP_QUERY,
             method,
             endpoint,
             header == NULL ? "" : (const char*)header);

    SequansController.writeCommand(command);

    char http_response_buffer[HTTP_RESPONSE_MAX_LENGTH]                = "";
    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";
    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH]              = "";

    if (!SequansController.waitForURC(HTTP_RING_URC,
                                      http_response_buffer,
                                      sizeof(http_response_buffer))) {
        Log.warn("Did not get HTTP response before timeout\r\n");
        return http_response;
    }

    bool got_response_code = SequansController.extractValueFromCommandResponse(
        http_response_buffer,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1,
        (char)NULL);

    bool got_data_size = SequansController.extractValueFromCommandResponse(
        http_response_buffer,
        HTTP_RESPONSE_DATA_SIZE_INDEX,
        data_size_buffer,
        HTTP_RESPONSE_DATA_SIZE_LENGTH,
        (char)NULL);

    if (got_response_code) {
        http_response.status_code = atoi(http_status_code_buffer);
    }

    if (got_data_size) {
        http_response.data_size = atoi(data_size_buffer);
    }

    return http_response;
}

bool HttpClientClass::configure(const char* host,
                                const uint16_t port,
                                const bool enable_tls) {

    if (enable_tls) {
        if (!SecurityProfile.profileExists(HTTPS_SECURITY_PROFILE_NUMBER)) {
            Log.error("Security profile not set up for HTTPS. Run the "
                      "'provision' Arduino sketch example to set this up.");

            return false;
        }
    }

    char command[HTTP_CONFIGURE_SIZE] = "";
    sprintf(command, HTTP_CONFIGURE, host, port, enable_tls ? 1 : 0);
    return SequansController.writeCommand(command) == ResponseResult::OK;
}

HttpResponse HttpClientClass::post(const char* endpoint,
                                   const uint8_t* data_buffer,
                                   const uint32_t data_length,
                                   const uint8_t* header_buffer,
                                   const uint32_t header_length,
                                   const ContentType content_type) {

    // The content type within the Sequans modem is classified by a single
    // character (+1 for NULL termination)
    char content_type_buffer[2] = "";

    switch (content_type) {
    case CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED:
        strncpy(content_type_buffer,
                HTTP_CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED,
                sizeof(content_type_buffer));
        break;

    case CONTENT_TYPE_APPLICATION_OCTET_STREAM:
        strncpy(content_type_buffer,
                HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM,
                sizeof(content_type_buffer));
        break;

    case CONTENT_TYPE_MULTIPART_FORM_DATA:
        strncpy(content_type_buffer,
                HTTP_CONTENT_TYPE_APPLICATION_MULTIPART_FORM_DATA,
                sizeof(content_type_buffer));
        break;

    case CONTENT_TYPE_APPLICATION_JSON:
        strncpy(content_type_buffer,
                HTTP_CONTENT_TYPE_APPLICATION_APPLICATION_JSON,
                sizeof(content_type_buffer));
        break;

    default:
        strncpy(content_type_buffer,
                HTTP_CONTENT_TYPE_TEXT_PLAIN,
                sizeof(content_type_buffer));
        break;
    }

    return sendData(endpoint,
                    data_buffer,
                    data_length,
                    HTTP_POST_METHOD,
                    header_buffer,
                    header_length,
                    content_type_buffer);
}

HttpResponse HttpClientClass::post(const char* endpoint,
                                   const char* data,
                                   const char* header,
                                   const ContentType content_type) {
    return post(endpoint,
                (uint8_t*)data,
                strlen(data),
                (uint8_t*)header,
                header == NULL ? 0 : strlen(header),
                content_type);
}

HttpResponse HttpClientClass::put(const char* endpoint,
                                  const uint8_t* data_buffer,
                                  const uint32_t data_length,
                                  const uint8_t* header_buffer,
                                  const uint32_t header_length) {
    return sendData(endpoint,
                    data_buffer,
                    data_length,
                    HTTP_PUT_METHOD,
                    header_buffer,
                    header_length);
}

HttpResponse HttpClientClass::put(const char* endpoint,
                                  const char* message,
                                  const char* header) {
    return put(endpoint,
               (uint8_t*)message,
               strlen(message),
               (uint8_t*)header,
               header == NULL ? 0 : strlen(header));
}

HttpResponse HttpClientClass::get(const char* endpoint, const char* header) {
    return queryData(endpoint,
                     HTTP_GET_METHOD,
                     (uint8_t*)header,
                     strlen(header));
}

HttpResponse HttpClientClass::head(const char* endpoint, const char* header) {
    return queryData(endpoint,
                     HTTP_HEAD_METHOD,
                     (uint8_t*)header,
                     strlen(header));
}

HttpResponse HttpClientClass::del(const char* endpoint, const char* header) {
    return queryData(endpoint,
                     HTTP_DELETE_METHOD,
                     (uint8_t*)header,
                     strlen(header));
}

int16_t HttpClientClass::readBody(char* buffer, const uint32_t buffer_size) {

    // Safeguard against the limitation in the Sequans AT command parameter
    // for the response receive command.
    if (buffer_size < HTTP_BODY_BUFFER_MIN_SIZE ||
        buffer_size > HTTP_BODY_BUFFER_MAX_SIZE) {
        return -1;
    }

    // Fix for bringing the modem out of idling and prevent timeout whilst
    // waiting for modem response during the next AT command
    SequansController.writeCommand("AT");

    // We send the buffer size with the receive command so that we only
    // receive that. The rest will be flushed from the modem.
    char command[HTTP_RECEIVE_LENGTH] = "";
    sprintf(command, HTTP_RECEIVE, buffer_size);
    SequansController.writeBytes((uint8_t*)command, strlen(command), true);

    // We receive three start bytes of the character '<', so we wait for
    // them
    uint8_t start_bytes = 3;

    while (start_bytes > 0) {
        if (SequansController.readByte() == HTTP_RECEIVE_START_CHARACTER) {
            start_bytes--;
        }
    }

    // Now we are ready to receive the payload. We only check for error and
    // not overflow in the receive buffer in comparison to our buffer as we
    // know the size of what we want to receive
    if (SequansController.readResponse(buffer, buffer_size) !=
        ResponseResult::OK) {
        return 0;
    }

    return strlen(buffer);
}

String HttpClientClass::readBody(const uint32_t size) {
    char buffer[size];
    int16_t bytes_read = readBody(buffer, sizeof(buffer));

    if (bytes_read == -1) {
        return "";
    }

    return String(buffer);
}

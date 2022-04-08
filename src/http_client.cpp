#include "http_client.h"
#include "log.h"
#include "sequans_controller.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// We only use profile 0 to keep things simple we also stick with spId 3
// which we dedicate to HTTPS
#define HTTP_CONFIGURE "AT+SQNHTTPCFG=0,\"%s\",%u,0,\"\",\"\",%u,120,1,3"

// Command without any data in it (with parantheses): 36 bytes
// Max length of doman name: 127 bytes
// Max length of port number: 5 bytes (0-65535)
// TLS enabled: 1 byte
// Termination: 1 byte
// This results in 36 + 127 + 5 + 1 + 1 = 170
#define HTTP_CONFIGURE_SIZE 170

#define HTTP_SEND    "AT+SQNHTTPSND=0,%u,\"%s\",%lu"
#define HTTP_RECEIVE "AT+SQNHTTPRCV=0,%lu"
#define HTTP_QUERY   "AT+SQNHTTPQRY=0,%u,\"%s\""

#define HTTP_POST_METHOD   0
#define HTTP_PUT_METHOD    1
#define HTTP_GET_METHOD    0
#define HTTP_HEAD_METHOD   1
#define HTTP_DELETE_METHOD 2

#define HTTP_RECEIVE_LENGTH          32
#define HTTP_RECEIVE_START_CHARACTER '<'
#define HTTP_SEND_START_CHARACTER    '>'

#define HTTP_RESPONSE_MAX_LENGTH         128
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

#define DEFAULT_RETRIES 5

HttpClientClass HttpClient = HttpClientClass::instance();

/**
 * @brief Waits for the HTTP response (which can't be requested) puts it into a
 * buffer.
 *
 * Since we can't query the response, and it will arrive as a single line of
 * string, we do the trick of sending a single AT command after we first see
 * that the receive buffer is not empty. The AT command will only give "OK" in
 * response, but we can use that as a termination for the HTTP response.
 *
 * @param buffer Buffer to place the HTTP response in.
 * @param buffer_size Size of buffer to place HTTP response in.
 *
 * @return Relays the return code from SequansController.readResponse().
 *         ResponseResult::OK if ok.
 */
static ResponseResult waitAndRetrieveHttpResponse(char *buffer,
                                                  const size_t buffer_size) {
    // Wait until the receive buffer is filled with the URC
    while (SequansController.readByte() != URC_IDENTIFIER_START_CHARACTER) {}

    // Send single AT command in order to receive an OK which will later will be
    // searched for as the termination in the HTTP response
    SequansController.writeCommand("AT");

    return SequansController.readResponse(buffer, buffer_size);
}

/**
 * @brief Generic method for sending data via HTTP, either with POST or PUT.
 * Issues an AT command to the LTE modem.
 *
 * @param endpoint Destination of payload, part after host name in URL.
 * @param buffer Payload to send.
 * @param buffer_size Size of payload.
 * @param method POST(0) or PUT(1).
 */
static HttpResponse sendData(const char *endpoint,
                             const uint8_t *buffer,
                             const uint32_t buffer_size,
                             const uint8_t method) {

    HttpResponse httpResponse = {0, 0};

    SequansController.clearReceiveBuffer();

    // Setup and transmit SEND command before sending the data
    const uint32_t digits_in_data_length = trunc(log10(buffer_size)) + 1;

    char command[strlen(HTTP_SEND) + strlen(endpoint) + digits_in_data_length];
    sprintf(command, HTTP_SEND, method, endpoint, (unsigned long)buffer_size);
    SequansController.writeCommand(command);

    // We receive one start bytes of the character '>', so we wait for
    // it
    while (SequansController.readByte() != HTTP_SEND_START_CHARACTER) {}

    // Now we deliver the payload
    SequansController.writeBytes(buffer, buffer_size);

    // Wait until we get some valid response and we don't reach the timeout for
    // the interface
    ResponseResult response_result;
    do {
        response_result = SequansController.readResponse();
    } while (response_result == ResponseResult::TIMEOUT);

    if (response_result != ResponseResult::OK) {
        return httpResponse;
    }

    char http_response[HTTP_RESPONSE_MAX_LENGTH] = "";
    if (waitAndRetrieveHttpResponse(http_response, HTTP_RESPONSE_MAX_LENGTH) !=
        ResponseResult::OK) {
        return httpResponse;
    }

    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";

    bool got_response_code = SequansController.extractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1);

    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH] = "";

    bool got_data_size = SequansController.extractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_DATA_SIZE_INDEX,
        data_size_buffer,
        HTTP_RESPONSE_DATA_SIZE_LENGTH);

    if (got_response_code) {
        httpResponse.status_code = atoi(http_status_code_buffer);
    }

    if (got_data_size) {
        httpResponse.data_size = atoi(data_size_buffer);
    }

    return httpResponse;
}

/**
 * @brief Generic method for retrieving data via HTTP, either with HEAD, GET or
 * DELETE.
 *
 * @param endpoint Destination of retrieve, part after host name in URL.
 * @param method GET(0), HEAD(1) or DELETE(2).
 */
static HttpResponse queryData(const char *endpoint, const uint8_t method) {

    HttpResponse httpResponse = {0, 0};

    SequansController.clearReceiveBuffer();

    // Set up and send the query
    char command[strlen(HTTP_QUERY) + strlen(endpoint)];
    sprintf(command, HTTP_QUERY, method, endpoint);

    if (!SequansController.retryCommand(command)) {
        Log.error("HTTP setting domain endpoint failed\r\n");
        return httpResponse;
    }

    char http_response[HTTP_RESPONSE_MAX_LENGTH] = "";
    if (waitAndRetrieveHttpResponse(http_response, HTTP_RESPONSE_MAX_LENGTH) !=
        ResponseResult::OK) {
        Log.error("HTTP response was not OK\r\n");
        return httpResponse;
    }

    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";

    bool got_response_code = SequansController.extractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1);

    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH] = "";

    bool got_data_size = SequansController.extractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_DATA_SIZE_INDEX,
        data_size_buffer,
        HTTP_RESPONSE_DATA_SIZE_LENGTH);

    if (got_response_code) {
        httpResponse.status_code = atoi(http_status_code_buffer);
    }

    if (got_data_size) {
        httpResponse.data_size = atoi(data_size_buffer);
    }

    return httpResponse;
}

bool HttpClientClass::configure(const char *host,
                                const uint16_t port,
                                const bool enable_tls) {

    SequansController.clearReceiveBuffer();

    char command[HTTP_CONFIGURE_SIZE] = "";
    sprintf(command, HTTP_CONFIGURE, host, port, enable_tls ? 1 : 0);

    return SequansController.retryCommand(command, DEFAULT_RETRIES);
}

HttpResponse HttpClientClass::post(const char *endpoint,
                                   const uint8_t *buffer,
                                   const uint32_t buffer_size) {
    return sendData(endpoint, buffer, buffer_size, HTTP_POST_METHOD);
}

HttpResponse HttpClientClass::post(const char *endpoint, const char *message) {
    return post(endpoint, (uint8_t *)message, strlen(message));
}

HttpResponse HttpClientClass::put(const char *endpoint,
                                  const uint8_t *buffer,
                                  const uint32_t buffer_size) {
    return sendData(endpoint, buffer, buffer_size, HTTP_PUT_METHOD);
}

HttpResponse HttpClientClass::put(const char *endpoint, const char *message) {
    return put(endpoint, (uint8_t *)message, strlen(message));
}

HttpResponse HttpClientClass::get(const char *endpoint) {
    return queryData(endpoint, HTTP_GET_METHOD);
}

HttpResponse HttpClientClass::head(const char *endpoint) {
    return queryData(endpoint, HTTP_HEAD_METHOD);
}

HttpResponse HttpClientClass::del(const char *endpoint) {
    return queryData(endpoint, HTTP_DELETE_METHOD);
}

int16_t HttpClientClass::readBody(char *buffer, const uint32_t buffer_size) {

    // Safeguard against the limitation in the Sequans AT command parameter
    // for the response receive command.
    if (buffer_size < HTTP_BODY_BUFFER_MIN_SIZE ||
        buffer_size > HTTP_BODY_BUFFER_MAX_SIZE) {
        return -1;
    }

    SequansController.clearReceiveBuffer();

    // We send the buffer size with the receive command so that we only
    // receive that. The rest will be flushed from the modem.
    char command[HTTP_RECEIVE_LENGTH] = "";

    sprintf(command, HTTP_RECEIVE, buffer_size);
    SequansController.writeCommand(command);

    // We receive three start bytes of the character '<', so we wait for
    // them
    uint8_t start_bytes = 3;

    // Wait for first byte in receive buffer
    while (!SequansController.isRxReady()) {}

    while (start_bytes > 0) {

        if (SequansController.readByte() == HTTP_RECEIVE_START_CHARACTER) {
            start_bytes--;
        }
    }

    // Now we are ready to receive the payload. We only check for error and
    // not overflow in the receive buffer in comparison to our buffer as we
    // know the size of what we want to receive
    if (SequansController.readResponse(buffer, buffer_size) ==
        ResponseResult::ERROR) {
        return 0;
    }

    size_t response_length = strlen(buffer);

    // Remove extra <CR> from command response
    buffer[response_length - 1] = '\0';

    return response_length - 1;
}

String HttpClientClass::readBody(const uint32_t size) {
    char buffer[size];
    int16_t bytes_read = readBody(buffer, sizeof(buffer));

    if (bytes_read == -1) {
        return "";
    }

    return String(buffer);
}

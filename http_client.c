#include "http_client.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sequans_controller.h"

// We only use profile 0 to keep things simple
// TODO/INPUT WANTED: Should we allow for more profiles?
#define HTTP_CONFIGURE "AT+SQNHTTPCFG=0,\"%s\",%d"

// This will support the max length of domain name of 127 characters as the
// sequence with the port number of at max 5 digits is 24 characters. So
// 24 + 127 + 1 (termination byte) = 152
#define HTTP_CONFIGURE_SIZE 152

#define HTTP_SEND "AT+SQNHTTPSND=0,%u,\"%s\",%u"
#define HTTP_POST_METHOD 0
#define HTTP_PUT_METHOD 1

#define HTTP_QUERY "AT+SQNHTTPQRY=0,%u,\"%s\""
#define HTTP_GET_METHOD 0
#define HTTP_HEAD_METHOD 1
#define HTTP_DELETE_METHOD 2

#define HTTP_RECEIVE "AT+SQNHTTPRCV=0,%u"
#define HTTP_RECEIVE_LENGTH 32
#define HTTP_RECEIVE_START_CHARACTER '<'

#define HTTP_RESPONSE_MAX_LENGTH 128
#define HTTP_RESPONSE_STATUS_CODE_INDEX 1
#define HTTP_RESPONSE_STATUS_CODE_LENGTH 3
#define HTTP_RESPONSE_DATA_SIZE_INDEX 3
#define HTTP_RESPONSE_DATA_SIZE_LENGTH 16

#define AT_DEFAULT_COMMAND "AT"

/**
 * @brief Waits for the HTTP response (which can't be requested) puts it into a
 *        buffer.
 *
 * Since we can't query the response, and it will arrive as a single line of
 * string, we do the trick of sending a single AT command after we first see
 * that the receive buffer is not empty. The AT command will only give "OK" in
 * response, but we can use that as a termination for the HTTP response.
 *
 * @param buffer Buffer to place the HTTP response in.
 * @param buffer_size Size of buffer to place HTTP response in.
 *
 * @return Relays the return code from sequansControllerFlushResponse().
 *         SEQUANS_CONTROLLER_RESPONSE_OK if ok.
 */
static uint8_t waitAndRetrieveHttpResponse(char *buffer,
                                           const size_t buffer_size) {
    // Wait until the receive buffer is filled with something from the HTTP
    // response
    while (!sequansControllerIsRxReady()) {}

    // Send single AT command in order to receive an OK which will later will be
    // searched for as the termination in the HTTP response
    sequansControllerSendCommand(AT_DEFAULT_COMMAND);

    // Read response will block until we read the OK and give us the HTTP
    // response
    uint8_t result = sequansControllerReadResponse(buffer, buffer_size);

    return result;
}

/**
 * @brief Generic method for sending data via HTTP, either with POST or PUT.
 *        Issues an AT command to the LTE modem.
 *
 * @param endpoint Destination of payload, part after host name in URL.
 * @param data Payload to send.
 * @param method POST(0) or PUT(1).
 *
 * @return HTTP status code from the request.
 */
static HttpResponse
sendData(const char *endpoint, const char *data, const uint8_t method) {

    HttpResponse httpResponse = {0, 0};

    // Clear the receive buffer to be ready for the response
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // Setup and transmit SEND command before sending the data
    const uint32_t data_lenth = strlen(data);
    const uint32_t digits_in_data_length = trunc(log10(data_lenth)) + 1;

    char command[strlen(HTTP_SEND) + strlen(endpoint) + digits_in_data_length];
    sprintf(command, HTTP_SEND, method, endpoint, data_lenth);
    sequansControllerSendCommand(command);

    // Now we deliver the payload
    sequansControllerSendCommand(data);
    if (sequansControllerFlushResponse() != SEQUANS_CONTROLLER_RESPONSE_OK) {
        return httpResponse;
    }

    char http_response[HTTP_RESPONSE_MAX_LENGTH] = "";
    if (waitAndRetrieveHttpResponse(http_response, HTTP_RESPONSE_MAX_LENGTH) !=
        SEQUANS_CONTROLLER_RESPONSE_OK) {
        return httpResponse;
    }

    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";

    uint8_t got_response_code =
        sequansControllerExtractValueFromCommandResponse(
            http_response,
            HTTP_RESPONSE_STATUS_CODE_INDEX,
            http_status_code_buffer,
            HTTP_RESPONSE_STATUS_CODE_LENGTH + 1);

    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH] = "";

    uint8_t got_data_size = sequansControllerExtractValueFromCommandResponse(
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

static HttpResponse queryData(const char *endpoint, const uint8_t method) {

    HttpResponse httpResponse = {0, 0};

    // Clear the receive buffer to be ready for the response
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // Set up and send the query
    char command[strlen(HTTP_QUERY) + strlen(endpoint)];
    sprintf(command, HTTP_QUERY, method, endpoint);
    sequansControllerSendCommand(command);

    if (sequansControllerFlushResponse() != SEQUANS_CONTROLLER_RESPONSE_OK) {
        return httpResponse;
    }

    char http_response[HTTP_RESPONSE_MAX_LENGTH] = "";
    if (waitAndRetrieveHttpResponse(http_response, HTTP_RESPONSE_MAX_LENGTH) !=
        SEQUANS_CONTROLLER_RESPONSE_OK) {
        return httpResponse;
    }

    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";

    bool got_response_code = sequansControllerExtractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1);

    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH] = "";

    bool got_data_size = sequansControllerExtractValueFromCommandResponse(
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

void httpClientConfigure(const char *host, uint16_t port) {

    uint8_t result;
    char command[HTTP_CONFIGURE_SIZE] = "";
    sprintf(command, HTTP_CONFIGURE, host, port);

    do {
        sequansControllerSendCommand(command);
        result = sequansControllerFlushResponse();
    } while (result != SEQUANS_CONTROLLER_RESPONSE_OK);
}

HttpResponse httpClientPost(const char *endpoint, const char *data) {
    return sendData(endpoint, data, HTTP_POST_METHOD);
}

HttpResponse httpClientPut(const char *endpoint, const char *data) {
    return sendData(endpoint, data, HTTP_PUT_METHOD);
}

HttpResponse httpClientGet(const char *endpoint) {
    return queryData(endpoint, HTTP_GET_METHOD);
}

HttpResponse httpClientHead(const char *endpoint) {
    return queryData(endpoint, HTTP_HEAD_METHOD);
}

HttpResponse httpClientDelete(const char *endpoint) {
    return queryData(endpoint, HTTP_DELETE_METHOD);
}

bool httpClientReadResponseBody(char *buffer, const uint32_t buffer_size) {
    // Clear the receive buffer to be ready for the response
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // We send the buffer size with the receive command so that we only
    // receive that. The rest will be flushed from the modem.
    char command[HTTP_RECEIVE_LENGTH] = "";
    sprintf(command, HTTP_RECEIVE, buffer_size);
    sequansControllerSendCommand(command);

    // We receive three start bytes of the character '<', so we wait for them
    uint8_t start_bytes = 3;

    // Wait for first byte in receive buffer
    while (!sequansControllerIsRxReady()) {}

    while (start_bytes > 0) {

        // This will block until we receive
        if (sequansControllerReadByte() == HTTP_RECEIVE_START_CHARACTER) {
            start_bytes--;
        }
    }

    // Now we are ready to receive the payload
    if (sequansControllerReadResponse(buffer, buffer_size) !=
        SEQUANS_CONTROLLER_RESPONSE_OK) {

        return false;
    }

    return true;
}

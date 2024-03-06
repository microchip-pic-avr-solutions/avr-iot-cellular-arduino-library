#include "http_client.h"
#include "flash_string.h"
#include "led_ctrl.h"
#include "log.h"
#include "security_profile.h"
#include "sequans_controller.h"
#include "timeout_timer.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

#define HTTPS_SECURITY_PROFILE_NUMBER (3)

#define HTTP_POST_METHOD   (0)
#define HTTP_PUT_METHOD    (1)
#define HTTP_GET_METHOD    (0)
#define HTTP_HEAD_METHOD   (1)
#define HTTP_DELETE_METHOD (2)

#define HTTP_RESPONSE_MAX_LENGTH         (84)
#define HTTP_RESPONSE_STATUS_CODE_INDEX  (1)
#define HTTP_RESPONSE_STATUS_CODE_LENGTH (3)
#define HTTP_RESPONSE_DATA_SIZE_INDEX    (3)
#define HTTP_RESPONSE_DATA_SIZE_LENGTH   (16)

// These are limitations from the Sequans module, so the range of bytes we can
// receive with one call to the read body AT command has to be between these
// values. One thus has to call the function multiple times if the data size is
// greater than the max size
#define HTTP_BODY_BUFFER_MIN_SIZE (64)
#define HTTP_BODY_BUFFER_MAX_SIZE (1500)

#define HTTP_TIMEOUT (20000)

// Content type specifiers for POST requests for the AT+SQNHTTPSND command
const char HTTP_CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED[] PROGMEM = "0";
const char HTTP_CONTENT_TYPE_TEXT_PLAIN[] PROGMEM                        = "1";
const char HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM[] PROGMEM          = "2";
const char HTTP_CONTENT_TYPE_APPLICATION_MULTIPART_FORM_DATA[] PROGMEM   = "3";
const char HTTP_CONTENT_TYPE_APPLICATION_APPLICATION_JSON[] PROGMEM      = "4";

const char HTTP_RING_URC[] PROGMEM     = "SQNHTTPRING";
const char HTTP_SHUTDOWN_URC[] PROGMEM = "SQNHTTPSH";

HttpClientClass HttpClient = HttpClientClass::instance();

static volatile bool got_shutdown_callback   = false;
static volatile uint16_t shutdown_error_code = 0;

/**
 * @brief Registered as a callback for the HTTP shutdown URC.
 */
static void httpShutdownCallback(char* urc) {
    char error_code_buffer[8] = "";

    const bool got_error_code =
        SequansController.extractValueFromCommandResponse(
            urc,
            1,
            error_code_buffer,
            sizeof(error_code_buffer) - 1,
            0);

    if (!got_error_code) {
        return;
    }

    got_shutdown_callback = true;
    shutdown_error_code   = atoi(error_code_buffer);
}

/**
 * @brief Waits for the HTTP response URC from the modem and returns the HTTP
 * response codes. This function also checks for an abrupt shutdown of the HTTP
 * connection, where the shutdown URC is sent with a cURL status code.
 */
static HttpResponse waitForResponse(const uint32_t timeout_ms) {

    HttpResponse http_response = {0, 0, 0};

    char http_response_buffer[HTTP_RESPONSE_MAX_LENGTH]                = "";
    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";
    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH]              = "";

    const auto toggle_led_whilst_waiting = [] {
        LedCtrl.toggle(Led::DATA, true);
    };

    // If the request fails for some reason, we will retrieve the SQNHTTPSH
    // (shutdown) URC which has the reason for failure, so we want to listen for
    // that as well
    got_shutdown_callback = false;
    SequansController.registerCallback(FV(HTTP_SHUTDOWN_URC),
                                       httpShutdownCallback);

    if (!SequansController.waitForURC(FV(HTTP_RING_URC),
                                      http_response_buffer,
                                      sizeof(http_response_buffer),
                                      timeout_ms,
                                      toggle_led_whilst_waiting,
                                      500)) {
        LedCtrl.off(Led::DATA, true);

        SequansController.unregisterCallback(FV(HTTP_SHUTDOWN_URC));

        Log.warnf(F("Did not get HTTP response before timeout on %d ms. "
                    "Consider increasing the timeout.\r\n"),
                  timeout_ms);

        return http_response;
    }

    // We pass 0 as the start character here as the URC data will only
    // contain the payload, not the URC identifier
    const bool got_response_code =
        SequansController.extractValueFromCommandResponse(
            http_response_buffer,
            HTTP_RESPONSE_STATUS_CODE_INDEX,
            http_status_code_buffer,
            HTTP_RESPONSE_STATUS_CODE_LENGTH + 1,
            0);

    const bool got_data_size =
        SequansController.extractValueFromCommandResponse(
            http_response_buffer,
            HTTP_RESPONSE_DATA_SIZE_INDEX,
            data_size_buffer,
            HTTP_RESPONSE_DATA_SIZE_LENGTH,
            0);

    if (got_response_code) {
        http_response.status_code = atoi(http_status_code_buffer);

        // The modem reports 0 as the status code if the connection has been
        // shut down with an error
        if (http_response.status_code == 0) {
            // The request failed, check if we got a shutdown URC. We give the
            // URC some time to arrive here.
            TimeoutTimer timer(1000);

            while (!got_shutdown_callback && !timer.hasTimedOut()) {
                _delay_ms(1);
            }

            if (got_shutdown_callback) {
                if (shutdown_error_code != 0) {
                    Log.errorf(
                        F("HTTP request failed with curl error code: %d. "
                          "Please refer to libcurl's error codes for more "
                          "information.\r\n"),
                        shutdown_error_code);
                }

                http_response.curl_error_code = shutdown_error_code;
            }
        }
    }

    if (got_data_size) {
        http_response.data_size = atoi(data_size_buffer);
    }

    SequansController.unregisterCallback(FV(HTTP_SHUTDOWN_URC));

    LedCtrl.off(Led::DATA, true);

    return http_response;
}

/**
 * @brief Generic method for sending data via HTTP, either with POST or PUT.
 *
 * @param endpoint Destination of payload, part after host name in URL.
 * @param data Payload to send.
 * @param data_length Length of payload.
 * @param method POST(0) or PUT(1).
 * @param header Optional header.
 * @param timeout_ms Timeout in milliseconds for the transmission.
 */
static HttpResponse
sendData(const char* endpoint,
         const uint8_t* data,
         const uint32_t data_length,
         const uint8_t method,
         const char* header        = NULL,
         const char* content_type  = "",
         const uint32_t timeout_ms = HTTP_DEFAULT_TIMEOUT_MS) {

    LedCtrl.on(Led::CON, true);

    // The modem could hang if several HTTP requests are done quickly after each
    // other, this alleviates this
    SequansController.writeCommand(F("AT"));

    HttpResponse http_response = {0, 0, 0};

    if (!SequansController.writeString(
            F("AT+SQNHTTPSND=0,%u,\"%s\",%lu,\"%s\",\"%s\""),
            true,
            method,
            endpoint,
            (unsigned long)data_length,
            content_type,
            header == NULL ? "" : (const char*)header)) {
        Log.error(F("Was not able to write HTTP AT command\r\n"));
        return http_response;
    }

    // Only send the data payload if there is any
    if (data_length > 0) {

        // Modem responds with '>' when it is ready to receive the payload
        if (!SequansController.waitForByte('>', HTTP_TIMEOUT)) {
            Log.error(
                F("Timed out whilst waiting on delivering the HTTP "
                  "payload. Is the "
                  "server online? If you're using HTTPS, you might need to "
                  "provision with a different CA certificate."));

            LedCtrl.off(Led::CON, true);
            return http_response;
        }

        // Now we deliver the payload
        SequansController.writeBytes(data, data_length, true);
    }

    http_response = waitForResponse(timeout_ms);

    LedCtrl.off(Led::CON, true);

    return http_response;
}

/**
 * @brief Generic method for retrieving data via HTTP, either with HEAD, GET or
 * DELETE.
 *
 * @param endpoint Destination of retrieve, part after host name in URL.
 * @param method GET(0), HEAD(1) or DELETE(2).
 * @param header Optional header.
 * @param timeout_ms Timeout in milliseconds for the query.
 */
static HttpResponse queryData(const char* endpoint,
                              const uint8_t method,
                              const uint8_t* header,
                              const uint32_t timeout_ms) {

    LedCtrl.on(Led::CON, true);

    // The modem could hang if several HTTP requests are done quickly after each
    // other, this alleviates this
    SequansController.writeCommand(F("AT"));

    HttpResponse http_response = {0, 0, 0};

    const ResponseResult response = SequansController.writeCommand(
        F("AT+SQNHTTPQRY=0,%u,\"%s\",\"%s\""),
        NULL,
        0,
        method,
        endpoint,
        header == NULL ? "" : (const char*)header);

    if (response != ResponseResult::OK) {
        Log.errorf(F("Was not able to write HTTP AT command, error: %X\r\n"),
                   static_cast<uint8_t>(response));
        return http_response;
    }

    http_response = waitForResponse(timeout_ms);

    LedCtrl.off(Led::CON, true);

    return http_response;
}

bool HttpClientClass::configure(const char* host,
                                const uint16_t port,
                                const bool enable_tls) {

    if (enable_tls) {
        if (!SecurityProfile.profileExists(HTTPS_SECURITY_PROFILE_NUMBER)) {
            Log.error(F("Security profile not set up for HTTPS. Run the "
                        "'provision' Arduino sketch example to set this up."));

            return false;
        }
    }

    // We only use profile 0 to keep things simple we also stick with spId 3
    // which we dedicate to HTTPS
    return SequansController.writeCommand(
               F("AT+SQNHTTPCFG=0,\"%s\",%u,0,\"\",\"\",%u,120,,3"),
               NULL,
               0,
               host,
               port,
               enable_tls ? 1 : 0) == ResponseResult::OK;
}

HttpResponse HttpClientClass::post(const char* endpoint,
                                   const uint8_t* data_buffer,
                                   const uint32_t data_length,
                                   const uint8_t* header_buffer,
                                   const uint32_t header_length,
                                   const ContentType content_type,
                                   const uint32_t timeout_ms) {

    // The content type within the Sequans modem is classified by a single
    // character (+1 for NULL termination)
    char content_type_buffer[2] = "";

    switch (content_type) {
    case CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED:
        strcpy_P(content_type_buffer,
                 HTTP_CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED);
        break;

    case CONTENT_TYPE_APPLICATION_OCTET_STREAM:
        strcpy_P(content_type_buffer,
                 HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM);
        break;

    case CONTENT_TYPE_MULTIPART_FORM_DATA:
        strcpy_P(content_type_buffer,
                 HTTP_CONTENT_TYPE_APPLICATION_MULTIPART_FORM_DATA);
        break;

    case CONTENT_TYPE_APPLICATION_JSON:
        strcpy_P(content_type_buffer,
                 HTTP_CONTENT_TYPE_APPLICATION_APPLICATION_JSON);
        break;

    default:
        strcpy_P(content_type_buffer, HTTP_CONTENT_TYPE_TEXT_PLAIN);
        break;
    }

    char header[header_length + 1] = "";
    strncpy(header, (const char*)header_buffer, header_length);
    header[header_length] = '\0';

    return sendData(endpoint,
                    data_buffer,
                    data_length,
                    HTTP_POST_METHOD,
                    header,
                    content_type_buffer,
                    timeout_ms);
}

HttpResponse HttpClientClass::post(const char* endpoint,
                                   const char* data,
                                   const char* header,
                                   const ContentType content_type,
                                   const uint32_t timeout_ms) {
    return post(endpoint,
                (uint8_t*)data,
                strlen(data),
                (uint8_t*)header,
                header == NULL ? 0 : strlen(header),
                content_type,
                timeout_ms);
}

HttpResponse HttpClientClass::put(const char* endpoint,
                                  const uint8_t* data_buffer,
                                  const uint32_t data_length,
                                  const uint8_t* header_buffer,
                                  const uint32_t header_length,
                                  const uint32_t timeout_ms) {

    char header[header_length + 1] = "";
    strncpy(header, (const char*)header_buffer, header_length);
    header[header_length] = '\0';

    return sendData(endpoint,
                    data_buffer,
                    data_length,
                    HTTP_PUT_METHOD,
                    header,
                    "",
                    timeout_ms);
}

HttpResponse HttpClientClass::put(const char* endpoint,
                                  const char* message,
                                  const char* header,
                                  const uint32_t timeout_ms) {
    return put(endpoint,
               (uint8_t*)message,
               strlen(message),
               (uint8_t*)header,
               header == NULL ? 0 : strlen(header),
               timeout_ms);
}

HttpResponse HttpClientClass::get(const char* endpoint,
                                  const char* header,
                                  const uint32_t timeout_ms) {
    return queryData(endpoint, HTTP_GET_METHOD, (uint8_t*)header, timeout_ms);
}

HttpResponse HttpClientClass::head(const char* endpoint,
                                   const char* header,
                                   const uint32_t timeout_ms) {
    return queryData(endpoint, HTTP_HEAD_METHOD, (uint8_t*)header, timeout_ms);
}

HttpResponse HttpClientClass::del(const char* endpoint,
                                  const char* header,
                                  const uint32_t timeout_ms) {
    return queryData(endpoint,
                     HTTP_DELETE_METHOD,
                     (uint8_t*)header,
                     timeout_ms);
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
    SequansController.writeCommand(F("AT"));

    // We send the buffer size with the receive command so that we only
    // receive that. The rest will be flushed from the modem.
    if (!SequansController.writeString(F("AT+SQNHTTPRCV=0,%lu"),
                                       true,
                                       buffer_size)) {
        Log.error(F("Was not able to write HTTP read body AT command\r\n"));
        return -1;
    }

    // We receive three start bytes '<', have to wait for them
    uint8_t start_bytes = 3;
    while (start_bytes > 0) {
        if (SequansController.readByte() == '<') {
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

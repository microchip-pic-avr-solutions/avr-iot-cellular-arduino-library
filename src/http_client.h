/**
 * @brief HTTP client for REST calls.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include <stdint.h>

#define HTTP_DEFAULT_TIMEOUT_MS (30000U)

typedef struct {
    uint16_t status_code;
    uint32_t data_size;
    uint16_t curl_error_code;
} HttpResponse;

class HttpClientClass {

  private:
    HttpClientClass(){};

  public:
    static HttpClientClass& instance(void) {
        static HttpClientClass instance;
        return instance;
    }

    enum StatusCodes {
        STATUS_OK                    = 200,
        STATUS_NOT_FOUND             = 404,
        STATUS_INTERNAL_SERVER_ERROR = 500,
    };

    enum ContentType {
        CONTENT_TYPE_APPLICATION_X_WWW_FORM_URLENCODED,
        CONTENT_TYPE_TEXT_PLAIN,
        CONTENT_TYPE_APPLICATION_OCTET_STREAM,
        CONTENT_TYPE_MULTIPART_FORM_DATA,
        CONTENT_TYPE_APPLICATION_JSON
    };

    /**
     * @brief Sets up the HTTP client with a host and port.
     *
     * @param host Can either be a host name resolved with DNS or an actual
     * server address in the form of "xxx.xxx.xxx.xxx".
     * @param port Port of the host, e.g. 80 or 443 (for HTTPS).
     * @param enable_tls Use tls, required for HTTPS.
     *
     * @return True if operation was successful.
     */
    bool
    configure(const char* host, const uint16_t port, const bool enable_tls);

    /**
     * @brief Issues a post to the host configured. Will block until operation
     * is done.
     *
     * @param endpoint Endpoint to issue the POST to. Is the part of the URL
     * after the domain.
     * @param data_buffer The data payload.
     * @param data_length The data length.
     * @param header_buffer Optional header line (e.g. for authorization
     * bearers)
     * @param header_length Length of the optinal header line.
     * @param content_type HTTP content type of the post request.
     * @param timeout_ms Timeout in milliseconds to wait for the POST request.
     */
    HttpResponse post(const char* endpoint,
                      const uint8_t* data_buffer,
                      const uint32_t data_length,
                      const uint8_t* header_buffer   = NULL,
                      const uint32_t header_length   = 0,
                      const ContentType content_type = CONTENT_TYPE_TEXT_PLAIN,
                      const uint32_t timeout_ms      = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Issues a post to the host configured. Will block until operation
     * is done.
     *
     * @param endpoint Endpoint to issue the POST to. Is the part of the URL
     * after the domain.
     * @param data The data payload.
     * @param header Optional header line (e.g. for authorization
     * bearers).
     * @param content_type HTTP content type of the post request.
     * @param timeout_ms Timeout in milliseconds to wait for the POST request.
     */
    HttpResponse post(const char* endpoint,
                      const char* data,
                      const char* header             = NULL,
                      const ContentType content_type = CONTENT_TYPE_TEXT_PLAIN,
                      const uint32_t timeout_ms      = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Issues a put to the host configured. Will block until operation is
     * done.
     *
     * @param endpoint Endpoint to issue the PUT to. Is the part of the URL
     * after the domain.
     * @param data_buffer The data payload.
     * @param data_length The data length.
     * @param header_buffer Optional header line (e.g. for authorization
     * bearers)
     * @param header_length Length of the optinal header line.
     * @param timeout_ms Timeout in milliseconds to wait for the PUT request.
     */
    HttpResponse put(const char* endpoint,
                     const uint8_t* data_buffer,
                     const uint32_t data_length,
                     const uint8_t* header_buffer = NULL,
                     const uint32_t header_length = 0,
                     const uint32_t timeout_ms    = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Issues a put to the host configured. Will block until operation is
     * done.
     *
     * @param endpoint Endpoint to issue the PUT to. Is the part of the URL
     * after the domain.
     * @param data The data payload.
     * @param header Optional header line (e.g. for authorization
     * bearers).
     * @param timeout_ms Timeout in milliseconds to wait for the PUT request.
     */
    HttpResponse put(const char* endpoint,
                     const char* data,
                     const char* header        = NULL,
                     const uint32_t timeout_ms = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Issues a get from the host configured. Will block until operation
     * is done. The contents of the body after the get can be read using the
     * readBody() function after.
     *
     * @param endpoint Endpoint to issue the PUT to. Is the part of the URL
     * after the domain.
     * @param header Optional header line (e.g. for authorization
     * bearers).
     * @param timeout_ms Timeout in milliseconds to wait for the GET request.
     */
    HttpResponse get(const char* endpoint,
                     const char* header        = NULL,
                     const uint32_t timeout_ms = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Issues a head from the host configured. Will block until operation
     * is done.
     *
     * @param endpoint Endpoint to issue the PUT to. Is the part of the URL
     * after the domain.
     * @param header Optional header line (e.g. for authorization
     * bearers).
     * @param timeout_ms Timeout in milliseconds to wait for the HEAD request.
     */
    HttpResponse head(const char* endpoint,
                      const char* header        = NULL,
                      const uint32_t timeout_ms = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Issues a delete from the host configured. Will block until
     * operation is done.
     *
     * @param endpoint Endpoint to issue the PUT to. Is the part of the URL
     * after the domain.
     * @param header Optional header line (e.g. for authorization
     * bearers).
     * @param timeout_ms Timeout in milliseconds to wait for the DELETE request.
     */
    HttpResponse del(const char* endpoint,
                     const char* header        = NULL,
                     const uint32_t timeout_ms = HTTP_DEFAULT_TIMEOUT_MS);

    /**
     * @brief Reads the body of a response after a HTTP call. Note that the
     * range for the buffer_size has to be between 64-1500. This is a limitation
     * from the Sequans LTE module. So if the data is larger than that,
     * multiple calls to this function has to be made.
     *
     * @param buffer Destination of the body.
     * @param buffer_size Has to be between 64-1500.
     *
     * @return bytes read from receive buffer. -1 indicates the buffer_size was
     * outside the range allowed.
     */
    int16_t readBody(char* buffer, const uint32_t buffer_size);

    /**
     * @brief Reads the body of the response after a HTTP call. Will read @param
     * size amount of bytes at a time, so several calls to this method has to be
     * made in order to read responses greater in size than that, or the size
     * has to be increased.
     *
     * @param size How many bytes to read at a time.
     */
    String readBody(const uint32_t size = 256);
};

extern HttpClientClass HttpClient;

#endif

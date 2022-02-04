/**
 * @brief HTTP client for REST calls.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include <stdint.h>

typedef struct {
    uint16_t status_code;
    uint32_t data_size;
} HttpResponse;

class HttpClientClass {

  private:
    HttpClientClass(){};

  public:
    static HttpClientClass &instance(void) {
        static HttpClientClass instance;
        return instance;
    }

    enum StatusCodes {
        STATUS_OK = 200,
        STATUS_NOT_FOUND = 404,
        STATUS_INTERNAL_SERVER_ERROR = 500,
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
    configure(const char *host, const uint16_t port, const bool enable_tls);

    /**
     * @brief Issues a post to the host configured. Will block until operation
     * is done.
     */
    HttpResponse post(const char *endpoint,
                      const uint8_t *buffer,
                      const uint32_t buffer_size);

    /**
     * @brief Issues a post to the host configured. Will block until operation
     * is done.
     *
     * @param messsage Needs to be a regular c string which is null terminated.
     *
     */
    HttpResponse post(const char *endpoint, const char *messsage);

    /**
     * @brief Issues a put to the host configured. Will block until operation is
     * done.
     */
    HttpResponse put(const char *endpoint,
                     const uint8_t *buffer,
                     const uint32_t buffer_size);

    /**
     * @brief Issues a put to the host configured. Will block until operation is
     * done.
     *
     * @param messsage Needs to be a regular c string which is null terminated.
     */
    HttpResponse put(const char *endpoint, const char *message);

    /**
     * @brief Issues a get from the host configured. Will block until operation
     * is done. The contents of the body after the get can be read using the
     * readBody() function after.
     */
    HttpResponse get(const char *endpoint);

    /**
     * @brief Issues a head from the host configured. Will block until operation
     * is done.
     */
    HttpResponse head(const char *endpoint);

    /**
     * @brief Issues a delete from the host configured. Will block until
     * operation is done.
     */
    HttpResponse del(const char *endpoint);

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
    int16_t readBody(char *buffer, const uint32_t buffer_size);

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

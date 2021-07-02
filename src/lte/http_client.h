/**
 * @brief HTTP client for REST calls.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t status_code;
    uint32_t data_size;
} HttpResponse;

/**
 * @brief Sets up the HTTP client with a host and port.
 *
 * @param host Can either be a host name resolved with DNS or an actual server
 * address in the form of "xxx.xxx.xxx.xxx".
 * @param port Port of the host, e.g. 80 or 443 (for HTTPS).
 * @param enable_tls Use HTTPS.
 *
 * @return True if operation was successful.
 */
bool httpClientConfigure(const char *host,
                         const uint16_t port,
                         const bool enable_tls);

/**
 * @brief Issues a post to the host configured. Will block until operation is
 * done.
 */
HttpResponse httpClientPost(const char *endpoint, const char *data);

/**
 * @brief Issues a put to the host configured. Will block until operation is
 * done.
 */
HttpResponse httpClientPut(const char *endpoint, const char *data);

/**
 * @brief Issues a get from the host configured. Will block until operation is
 * done. The contents of the body after the get can be read using the
 * httpClientReadResponseBody() function after.
 */
HttpResponse httpClientGet(const char *endpoint);

/**
 * @brief Issues a head from the host configured. Will block until operation is
 * done.
 */
HttpResponse httpClientHead(const char *endpoint);

/**
 * @brief Issues a delete from the host configured. Will block until operation
 * is done.
 */
HttpResponse httpClientDelete(const char *endpoint);

/**
 * @brief Reads the body of a response after a HTTP call. Note that the range
 * for the buffer_size has to be between 64-1500. This is a limitation
 * from the Sequans LTE module. So if the data is larger than that,
 * multiple calls to this function has to be made.
 *
 * @param buffer Destination of the body.
 * @param buffer_size Has to be between 64-1500.
 *
 * @return bytes read from receive buffer. -1 indicates the buffer_size was
 * outside the range allowed.
 */
int16_t httpClientReadResponseBody(char *buffer, const uint32_t buffer_size);

#endif

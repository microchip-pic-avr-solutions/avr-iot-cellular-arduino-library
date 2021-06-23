/**
 * @brief HTTP client for REST calls.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

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
 *             address in the form of "xxx.xxx.xxx.xxx".
 * @param port Port of the host, e.g. 80.
 */
void httpClientConfigure(const char *host, uint16_t port);

/**
 * @brief Issues a post to the host configured. Will block until operation is
 *        done.
 *
 * @param endpoint Destination on host.
 * @param data Payload to send.
 *
 * @return Status of the operation.
 */
HttpResponse httpClientPost(const char *endpoint, const char *data);

/**
 * @brief Issues a put to the host configured. Will block until operation is
 *        done.
 *
 * @param endpoint Destination on host.
 * @param data Payload to send.
 *
 * @return Status of the operation.
 */
HttpResponse httpClientPut(const char *endpoint, const char *data);

/**
 * @brief Issues a get from the host configured. Will block until operation is
 *        done. The contents of the body after the get can be read using the
 *        httpClientReadResponseBody() function after.
 *
 * @param endpoint Destination on host.
 *
 * @return Status of the operation.
 */
HttpResponse httpClientGet(const char *endpoint);

/**
 * @brief Issues a head from the host configured. Will block until operation is
 *        done.
 *
 * @param endpoint Destination on host.
 *
 * @return Status of the operation.
 */
HttpResponse httpClientHead(const char *endpoint);

/**
 * @brief Issues a delete from the host configured. Will block until operation
 *        is done.
 *
 * @param endpoint Destination on host.
 *
 * @return Status of the operation.
 */

HttpResponse httpClientDelete(const char *endpoint);

/**
 * @brief Reads the body of a response after a HTTP call. The contents after the
 *        buffer size will be discarded, so passing the data size retrieved from
 *        the HttpResponse plus some space for termination of the response from
 *        LTE module is recommended to get the whole buffer if that is required.
 *        Use e.g. buffer_size = httpResponse.data_size + 32;
 *
 * @param buffer Destination of the body.
 * @param buffer_size Max size of the buffer,
 *
 * @return true if successful read.
 */
bool httpClientReadResponseBody(char *buffer, const uint32_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif

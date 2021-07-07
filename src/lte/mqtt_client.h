/**
 * @brief MQTT client for connecting to e.g AWS.
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief [...] Will disconnect from the broker before configuring the new
 * client.
 *
 *
 */
bool mqttClientConfigure(const char *client_id, const bool use_tls);

bool mqttClientConnect(const char *host, const uint16_t port);

bool mqttClientDisconnect(void);

bool mqttClientPublish(const char *topic,
                       const uint8_t qos,
                       const uint8_t *buffer,
                       const uint32_t buffer_size);

bool mqttClientSubscribe(const char *topic, const uint8_t qos);

#endif

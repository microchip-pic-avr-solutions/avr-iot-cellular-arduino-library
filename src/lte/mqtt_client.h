/**
 * @brief MQTT client for connecting to e.g AWS.
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

class MqttClient {

  public:
    /**
     * @brief [...] Will disconnect from the broker before configuring the new
     * client.
     *
     *
     */
    bool begin(const char *client_id,
               const char *host,
               const uint16_t port,
               const bool use_tls);

    bool end(void);

    bool publish(const char *topic,
                 const uint8_t qos,
                 const uint8_t *buffer,
                 const uint32_t buffer_size);

    bool subscribe(const char *topic, const uint8_t qos);
};

#endif

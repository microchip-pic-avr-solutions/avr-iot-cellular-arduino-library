/**
 * @brief Singleton MQTT client for connecting to e.g AWS.
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    String receive_topic;
    uint16_t message_length;

} MqttReceiveNotification;

typedef enum {
    AT_MOST_ONCE = 0, // default
    AT_LEAST_ONCE,
    EXACTLY_ONCE
} MqttQoS;

class MqttClientClass {

  private:
    /**
     * @brief Hide constructor in order to enforce a single instance of the
     * class.
     */
    MqttClientClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static MqttClientClass &instance(void) {
        static MqttClientClass instance;
        return instance;
    }

    /**
     * @brief Will configure and connect to the host/broker
     * specified. Will disconnect from the current broker (if any) before
     * configuring the new client.
     *
     * @param client_id The identifier for this unit.
     * @param host Host/broker to attempt to connect to.
     * @param port Port for communication.
     * @param use_tls Whether to use TLS in the communication.
     * @param keep_alive How often the broker is pinged. If low power is
     * utilised, the modem will wake up every @p keep_alive to ping the broker
     * regardless of the sleeping time.
     * @param use_ecc Whether to use the ECC for signing messages. If not used,
     * the private key has to be stored on the LTE modem and the security
     * profile has to be be set up to not use external hardware cryptographic
     * engine.
     *
     * @return true if configuration and connection was succesful.
     */
    bool begin(const char *client_id,
               const char *host,
               const uint16_t port,
               const bool use_tls,
               const size_t keep_alive = 60,
               const bool use_ecc = true);

    bool beginAWS();

    /**
     * @brief Disconnects from the broker.
     */
    bool end(void);

    /**
     * @brief Register callback function for when the client is
     * connected/disconnected to/from the MQTT broker. Called from ISR, so keep
     * this function short.
     */
    void onConnectionStatusChange(void (*connected)(void),
                                  void (*disconnected)(void));

    /**
     * @return true if connected to MQTT broker.
     */
    bool isConnected(void);

    /**
     * @brief Publishes the contents of the buffer to the given topic.
     *
     * @param topic Topic to publish to.
     * @param buffer Data to publish.
     * @param buffer_size Has to be in range 1-65535.
     * @param quality_of_service MQTT protocol QoS.
     *
     * @return true if publish was successful.
     */
    bool publish(const char *topic,
                 const uint8_t *buffer,
                 const uint32_t buffer_size,
                 const MqttQoS quality_of_service = AT_MOST_ONCE);

    /**
     * @brief Publishes the contents of the message to the given topic.
     *
     * @param topic Topic to publish to.
     * @param message String to publish, has to be null terminated.
     * @param quality_of_service MQTT protocol QoS.
     *
     * @return true if publish was successful.
     */
    bool publish(const char *topic,
                 const char *message,
                 const MqttQoS quality_of_service = AT_MOST_ONCE);

    /**
     * @brief Subscribes to a given topic.
     *
     * @param topic Topic to subscribe to.
     * @param quality_of_service MQTT protocol QoS.
     *
     * @return true if subscription was successful.
     */
    bool subscribe(const char *topic,
                   const MqttQoS quality_of_service = AT_MOST_ONCE);

    /**
     * @brief Register a callback function which will be called when we receive
     * a message on any topic we've subscribed on. Called from ISR, so keep this
     * function short.
     */
    void onReceive(void (*callback)(char *));

    /**
     * @brief Reads a receive notification (if any).
     *
     * @return Data from last receive notification. If no notification has
     * already been read and there are no new ones, a message length of 0 will
     * be returned.
     */
    MqttReceiveNotification readReceiveNotification(void);

    /**
     * @brief Reads the message received on the given topic (if any).
     *
     * @param topic topic message received on.
     * @param buffer Buffer to place message.
     * @param buffer_size Size of buffer. Max is 1024.
     *
     * @return true if message was read successfully. False if there were no
     * message or the message buffer was over 1024, which is a limitation from
     * the LTE module.
     */
    bool readMessage(const char *topic, uint8_t *buffer, uint16_t buffer_size);

    /**
     * @brief Reads the message received on the given topic (if any).
     *
     * @param size Size of buffer. Max is 1024.
     *
     * @return The message or an empty string if no new message was retrieved on
     * the given topic.
     */
    String readMessage(const char *topic, const uint16_t size = 256);

    /**
     * @brief If the MQTT connection is encrypted with the hardware crypto
     * engine, we need to regularly check and sign the data going back and
     * forward (for example when the unit is pinging the broker).
     *
     * @note This function has to be called regularly, for example in the main
     * loop for the MQTT connection to remain active.
     */
    bool signIncomingRequests(void);
};

extern MqttClientClass MqttClient;

#endif

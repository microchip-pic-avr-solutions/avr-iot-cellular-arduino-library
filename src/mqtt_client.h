/**
 * @brief Singleton MQTT client for connecting to e.g AWS.
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

#define MQTT_TOPIC_MAX_LENGTH (384)

typedef enum { AT_MOST_ONCE = 0, AT_LEAST_ONCE, EXACTLY_ONCE } MqttQoS;

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
    static MqttClientClass& instance(void) {
        static MqttClientClass instance;
        return instance;
    }

    /**
     * @brief Will configure and connect to the host/broker specified.
     *
     * @param client_id The identifier for this unit.
     * @param host Host/broker to attempt to connect to.
     * @param port Port for communication.
     * @param use_tls Whether to use TLS in the communication.
     * @param keep_alive Optional: How often the broker is pinged. If low power
     * is utilised, the modem will wake up every @p keep_alive to ping the
     * broker regardless of the sleeping time.
     * @param use_ecc Optional: Whether to use the ECC for signing messages. If
     * not used, the private key has to be stored on the LTE modem and the
     * security profile has to be be set up to not use external hardware
     * cryptographic engine.
     * @param username Optional: Username for authentication.
     * @param password Optional: Password for authentication.
     * @param timeout_ms: Timeout for connecting to the broker.
     * @param print_messages: If set to true, prints "Connecting to MQTT
     * broker..."
     *
     * @return true if configuration and connection was succesful.
     */
    bool begin(const char* client_id,
               const char* host,
               const uint16_t port,
               const bool use_tls,
               const uint16_t keep_alive = 1200,
               const bool use_ecc        = true,
               const char* username      = "",
               const char* password      = "",
               const size_t timeout_ms   = 30000,
               const bool print_messages = true);

    /**
     * @brief Will configure and connect to the provisioned AWS broker.
     */
    bool beginAWS(const uint16_t keep_alive = 1200);

    /**
     * @brief Will configure and connect to the provisioned Azure broker.
     */
    bool beginAzure(const uint16_t keep_alive = 1200);

    /**
     * @brief Disconnects from the broker and resets the state in the MQTT
     * client.
     */
    bool end();

    /**
     * @brief Register callback function for when the client is
     * connected/disconnected to/from the MQTT broker. Called from ISR, so keep
     * this function short.
     */
    __attribute__((deprecated(
        "onConnectionStatusChange is deprecated as of version 1.3.8 as "
        "MqttClient.begin() is now blocking and does not support "
        "connected callback anymore (it will not be called by the "
        "library). please use onDisconnect instead"))) void
    onConnectionStatusChange(void (*connected)(void),
                             void (*disconnected)(void));

    /**
     * @brief Register callback function for when the client is
     * disconnected from the MQTT broker. Called from ISR, so keep
     * this function short.
     */
    void onDisconnect(void (*disconnected)(void));

    /**
     * @return true if connected to MQTT broker.
     */
    bool isConnected();

    /**
     * @brief Publishes the contents of the buffer to the given topic.
     *
     * @param topic Topic to publish to.
     * @param buffer Data to publish.
     * @param buffer_size Has to be in range 1-65535.
     * @param quality_of_service MQTT protocol QoS.
     * @param timeout_ms Timeout waiting for publish confirmation.
     *
     * @return true if publish was successful.
     */
    bool publish(const char* topic,
                 const uint8_t* buffer,
                 const uint32_t buffer_size,
                 const MqttQoS quality_of_service = AT_LEAST_ONCE,
                 const uint32_t timeout_ms        = 30000);

    /**
     * @brief Publishes the contents of the message to the given topic.
     *
     * @param topic Topic to publish to.
     * @param message String to publish, has to be null terminated.
     * @param quality_of_service MQTT protocol QoS.
     * @param timeout_ms Timeout waiting for publish confirmation.
     *
     * @return true if publish was successful.
     */
    bool publish(const char* topic,
                 const char* message,
                 const MqttQoS quality_of_service = AT_LEAST_ONCE,
                 const uint32_t timeout_ms        = 30000);

    /**
     * @brief Subscribes to a given topic.
     *
     * @param topic Topic to subscribe to.
     * @param quality_of_service MQTT protocol QoS.
     *
     * @return true if subscription was successful.
     */
    bool subscribe(const char* topic,
                   const MqttQoS quality_of_service = AT_MOST_ONCE);

    /**
     * @brief Register a callback function which will be called when we receive
     * a message on any topic we've subscribed on. Called from ISR, so keep this
     * function short.
     *
     * @param message_id This value will be -1 if the MqttQoS is set to
     * AT_MOST_ONCE.
     */
    void onReceive(void (*callback)(const char* topic,
                                    const uint16_t message_length,
                                    const int32_t message_id));

    /**
     * @brief Reads the message received on the given topic (if any).
     *
     * @param topic topic message received on.
     * @param buffer Buffer to place message.
     * @param buffer_size Size of buffer. Max is 1024.
     * @param message_id If QoS is not MqttQoS::AT_MOST_ONCE, we get a message
     * ID during the callback. This has to be specified here. If this argument
     * is -1, message ID will not be passed when retrieving the message.
     *
     * @return true if message was read successfully. False if there were no
     * message or the message buffer was over 1024, which is a limitation from
     * the LTE module.
     */
    bool readMessage(const char* topic,
                     char* buffer,
                     const uint16_t buffer_size,
                     const int32_t message_id = -1);

    /**
     * @brief Reads the message received on the given topic (if any).
     *
     * @param size Size of buffer. Max is 1024.
     *
     * @return The message or an empty string if no new message was retrieved on
     * the given topic.
     */
    String readMessage(const char* topic, const uint16_t size = 256);

    /**
     * @brief Reads @p num_messages MQTT messages from the Sequans modem and
     * discards them.
     *
     * @param topic Topic to clear the messages from.
     * @param num_messages Number of messages to discard.
     */
    void clearMessages(const char* topic, const uint16_t num_messages);
};

extern MqttClientClass MqttClient;

#endif

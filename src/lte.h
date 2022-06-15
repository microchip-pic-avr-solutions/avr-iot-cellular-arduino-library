/**
 * @brief Higher level interface for interacting with the LTE module.
 */

#ifndef LTE_H
#define LTE_H

#include <stdint.h>

class LteClass {

  private:
    /**
     * @brief Constructor is hidden to enforce a single instance of this class
     * through a singleton.
     */
    LteClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static LteClass &instance(void) {
        static LteClass instance;
        return instance;
    }

    /**
     * @brief Initializes the LTE module and its controller interface. Connects
     * to the network.
     *
     * @param print_messages If set to true, the messages related to connection
     * will be logged.
     *
     * @return True if initialization was successful and connection was made.
     */
    bool begin(const bool print_messages = true);

    /**
     * @brief Disables the interface with the LTE module. Disconnects from
     * operator.
     */
    void end(void);

    /**
     * @return The operator name
     */
    String getOperator(void);

    /**
     * @brief Registers callback function for when the modem disconnected from
     * the operator.
     */
    void onDisconnect(void (*disconnect_callback)(void));

    bool isConnected(void);
};

extern LteClass Lte;

#endif

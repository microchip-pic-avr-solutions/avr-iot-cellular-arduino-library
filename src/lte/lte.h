/**
 * @brief Higher level interface for interacting with the LTE module.
 */

#ifndef LTE_H
#define LTE_H

class LTEClass {

  public:
    /**
     * @brief Initializes the LTE module and its controller interface. Starts
     * searching for operator.
     */
    void begin(void);

    /**
     * @brief Disables the interface with the LTE module. Disconnects from
     * operator.
     */
    void end(void);

    /**
     * @brief Registers a callback function which will be called when the
     * connection status is updated.
     */
    void registerConnectionNotificationCallback(void (*callback)(void));

    bool isConnectedToOperator(void);
};

extern LTEClass LTE;

#endif

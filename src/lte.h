/**
 * @brief Higher level interface for interacting with the LTE module.
 */

#ifndef LTE_H
#define LTE_H

class LteClass {

  private:
    LteClass(){};

  public:
    static LteClass &instance(void) {
        static LteClass instance;
        return instance;
    }

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
     * @brief Registers callback functions for when the module is connected to
     * the operator and disconnected from the operator.
     */
    void onConnectionStatusChange(void (*connect_callback)(void),
                                  void (*disconnect_callback)(void));

    bool isConnected(void);
};

extern LteClass Lte;

#endif

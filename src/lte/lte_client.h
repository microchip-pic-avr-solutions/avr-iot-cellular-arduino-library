/**
 * @brief Higher level interface for interacting with the LTE module.
 */

#ifndef LTE_CLIENT_H
#define LTE_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the LTE client and its controller interface.
 */
void lteClientBegin(void);

/**
 * @brief Disables the interface with the LTE module.
 */
void lteClientEnd(void);

/**
 * @brief Will request connection to operation. The action will not happen
 * instanteneously, so checking the connection status has to be done.
 *
 * @return true if request was successful.
 */
bool lteClientRequestConnectionToOperator(void);

/**
 * @return true if operation was successful.
 */
bool lteClientDisconnectFromOperator(void);

bool lteClientIsConnectedToOperator(void);

#endif

/**
 * @brief Higher level interface for interacting with the LTE module.
 */

#ifndef LTE_CLIENT_H
#define LTE_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the LTE client and its controller interface.
 */
void lteClientInitialize();

void lteClientEnableRoaming(void);

/**
 * @brief Will request connection to operation. The action will not happen
 *        instanteneously, so checking the connection status has to be done.
 */
void lteClientConnectToOperator(void);

bool lteClientIsConnectedToOperator(void);

void lteClientDisconnectFromOperator(void);

/**
 * @brief Will relay all messages back and forward between serial connected to
 *        host computer and the LTE module.
 */
void lteClientStartDebugBridgeMode(void);

#ifdef __cplusplus
}
#endif

#endif

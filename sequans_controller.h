/**
 * @brief Interface for sending AT commands to and receiveing responses from the
 *        Sequans GM02S module.
 */

#ifndef SEQUANS_CONTROLLER_H
#define SEQUANS_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEQUANS_CONTROLLER_RESPONSE_OK 0
#define SEQUANS_CONTROLLER_RESPONSE_ERROR 1
#define SEQUANS_CONTROLLER_BUFFER_OVERFLOW 2
#define SEQUANS_CONTROLLER_RESPONSE_TIMEOUT 3

#define SEQUANS_CONTROLLER_DEFAULT_FLUSH_RETRIES 5
#define SEQUANS_CONTROLLER_DEFAULT_FLUSH_SLEEP_MS 10

/**
 * @brief Sets up the pins for TX, RX, RTS and CTS of the USART interface
 * towards the LTE module. Will also issue a RESET for the LTE module.
 */
void sequansControllerInitialize(void);

/**
 * @return True when TX buffer is not full.
 */
bool sequansControllerIsTxReady(void);

/**
 * @return True when RX buffer is not full.
 */
bool sequansControllerIsRxReady(void);

/**
 * @return True when transmit has completed.
 */
bool sequansControllerIsTxDone(void);

/**
 * @brief Send a byte to the LTE module, will block if the transmit buffer is
 * full.
 */
void sequansControllerSendByte(const uint8_t data);

/**
 * @brief Writes an AT command in the form of a string to the LTE module, will
 * block if the transmit buffer is full.
 *
 * @note A carrige return is not needed for the command.
 */
void sequansControllerSendCommand(const char *command);

/**
 * @brief Reads a byte from the LTE UART interface. Will block.
 */
uint8_t sequansControllerReadByte(void);

/**
 * @brief Reads a response after e.g. an AT command. Will block!
 *
 * @note This function requires that the modem is in ATV1 mode. Which is the
 * default mode.
 *
 * If the response won't fit into the buffer size specified, the function will
 * just return the buffer overflow error code specified in the defines in the
 * top of this file and the out_buffer will be filled up to buffer_size.
 *
 * @param out_buffer Buffer to place the response.
 * @param buffer_size Max size of response bytes to read.
 *
 * @return - LTE_CONTROLLER_RESPONSE_OK if read was successfull and result
 *           was terminated by OK.
 *         - LTE_CONTROLLER_RESPONSE_ERROR if read was successfull but
 *           result was terminated by ERROR.
 *         - LTE_CONTROLLER_BUFFER_OVERFLOW if read resulted in buffer
 *           overflow.
 */
uint8_t sequansControllerReadResponse(char *out_buffer, uint16_t buffer_size);

/**
 * @brief Will read the response to flush out the receive buffer, but not place
 * the read bytes anywhere. Returns whether an OK or ERROR was found in
 * the response.
 *
 * This can be used where the response of a command is of interest, but
 * where thebuffer has to be cleared for the next time a command is given. This
 * can also be used to check if the result from a command was a "OK" or "ERROR"
 * without placing the actual result anywhere. Will read the receive buffer
 * until an "OK\r\n" or "ERROR\r\n" is found, which is the termination sequence
 * for the LTE modem in ATV1 mode (or if amount of retries is passed).
 *
 * The retry behaviour will check if there is anything in the receive buffer, if
 * not, the function will sleep and retry again until the amount of retries is
 * passed.
 *
 * @param retries Amount of times to retry reading from the LTE module. This is
 * to prevent the function from blocking the rest of the program if it is called
 * while the LTE module is not expected to transmit anything.
 *
 * @param sleep_us Amount of time to sleep in milliseconds between the retries.
 *
 * @return - LTE_CONTROLLER_RESPONSE_OK if termination was found to be "OK".
 *         - LTE_CONTROLLER_RESPONSE_ERROR if termination was found to be
 *           "ERROR".
 *         - LTE_CONTROLLER_RESPONSE_TIMEOUT if we passed the retry amount
 */
uint8_t sequansControllerFlushResponseWithRetries(const uint8_t retries,
                                                  const double sleep_ms);

/**
 * @brief See sequansControllerFlushResponseWithRetries(). Uses a default
 * retry value of SEQUANS_CONTROLLER_DEFAULT_FLUSH_RETRIES and sleep time
 * of SEQUANS_CONTROLLER_DEFAULT_FLUSH_SLEEP_MS.
 */
uint8_t sequansControllerFlushResponse(void);

/**
 * @brief Searches for a value at one index in the response, which has a comma
 * delimiter.
 *
 * @param response The AT command response.
 * @param index Index of value to extract.
 * @param buffer Destination buffer for value.
 * @param buffer_size Destination buffer size.
 *
 * @return true if extraction was successful.
 */
bool sequansControllerExtractValueFromCommandResponse(char *response,
                                                      const uint8_t index,
                                                      char *buffer,
                                                      const size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif

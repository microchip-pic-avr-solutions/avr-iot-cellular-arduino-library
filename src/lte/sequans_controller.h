/**
 * @brief Interface for sending AT commands to and receiveing responses from the
 *        Sequans GM02S module.
 */

#ifndef SEQUANS_CONTROLLER_H
#define SEQUANS_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OK,
    ERROR,
    BUFFER_OVERFLOW,
    TIMEOUT,
    SERIAL_READ_ERROR
} ResponseResult;

/**
 * @brief Sets up the pins for TX, RX, RTS and CTS of the serial interface
 * towards the LTE module. Will also issue a RESET for the LTE module.
 */
void sequansControllerBegin(void);

/**
 * @brief Disables interrupts used for the sequans module and closes the
 * serial interface.
 */
void sequansControllerEnd(void);

/**
 * @brief Specify for the methods which may fail how many times we ought to
 * retry and sleep between the retries.
 */
void sequansControllerSetRetryConfiguration(const uint8_t num_retries,
                                            const double sleep_ms);

bool sequansControllerIsTxReady(void);

bool sequansControllerIsRxReady(void);

/**
 * @return true if write was successful.
 */
bool sequansControllerWriteByte(const uint8_t data);

/**
 * @brief Writes an AT command in the form of a string to the LTE module,
 * will block if the transmit buffer is full.
 *
 * @note A carrige return is not needed for the command.
 *
 * @return true if write was successful.
 */
bool sequansControllerWriteCommand(const char *command);

/**
 * @return -1 if failed to read or read value if else.
 */
int16_t sequansControllerReadByte(void);

/**
 * @brief Reads a response after e.g. an AT command.
 *
 * @note This function requires that the modem is in ATV1 mode. Which is the
 * default mode.
 *
 * If the response won't fit into the buffer size specified, the function
 * will just return the buffer overflow error code specified in the defines
 * in the top of this file and the out_buffer will be filled up to
 * buffer_size.
 *
 * @param out_buffer Buffer to place the response.
 * @param buffer_size Max size of response bytes to read.
 *
 * @return - OK if read was successfull and resultw as terminated by OK.
 *         - ERROR if read was successfull but result was terminated by ERROR.
 *         - OVERFLOW if read resulted in buffer overflow.
 *         - SERIAL_READ_ERROR if an error occured in the serial interface
 */
ResponseResult sequansControllerReadResponse(char *out_buffer,
                                             uint16_t buffer_size);

/**
 * @brief Will read the response to flush out the receive buffer, but not
 * place the read bytes anywhere. Returns whether an OK or ERROR was found
 * in the response.
 *
 * This can be used where the response of a command is of interest, but
 * where thebuffer has to be cleared for the next time a command is given.
 * This can also be used to check if the result from a command was a "OK" or
 * "ERROR" without placing the actual result anywhere. Will read the receive
 * buffer until an "OK\r\n" or "ERROR\r\n" is found, which is the
 * termination sequence for the LTE modem in ATV1 mode (or if amount of
 * retries is passed).
 *
 * @return - OK if termination was found to be "OK".
 *         - ERROR if termination was found to be "ERROR".
 *         - TIMEOUT if we passed the retry amount.
 */
ResponseResult sequansControllerFlushResponse(void);

/**
 * @brief Searches for a value at one index in the response, which has a
 * comma delimiter.
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

#endif

/**
 * @brief Interface for sending AT commands to and receiveing responses from the
 *        Sequans GM02S module.
 */

#ifndef SEQUANS_CONTROLLER_H
#define SEQUANS_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Is public here for users of the interface
#define URC_DATA_BUFFER_SIZE 164

#define URC_IDENTIFIER_START_CHARACTER '+'
#define URC_IDENTIFIER_END_CHARACTER   ':'

typedef enum {
    OK,
    ERROR,
    BUFFER_OVERFLOW,
    TIMEOUT,
    SERIAL_READ_ERROR
} ResponseResult;

class SequansControllerClass {

  private:
    SequansControllerClass(){};

  public:
    static SequansControllerClass &instance(void) {
        static SequansControllerClass instance;
        return instance;
    }

    /**
     * @brief Sets up the pins for TX, RX, RTS and CTS of the serial interface
     * towards the LTE module. Will also issue a RESET for the LTE module.
     */
    void begin(void);

    /**
     * @brief Disables interrupts used for the sequans module and closes the
     * serial interface.
     */
    void end(void);

    /**
     * @brief Specify for the methods which may fail how many times we ought to
     * retry and sleep between the retries.
     */
    void setRetryConfiguration(const uint8_t num_retries,
                               const double sleep_ms);

    bool isTxReady(void);

    bool isRxReady(void);

    /**
     * @return true if write was successful.
     */
    bool writeByte(const uint8_t data);

    /**
     * @brief Writes a data buffer to the LTE modem.
     *
     * @return true if write was successful.
     */
    bool writeBytes(const uint8_t *data, const size_t buffer_size);

    /**
     * @brief Writes an AT command in the form of a string to the LTE module.
     *
     * @note A carrige return is not needed for the command as it is appended.
     *
     * @return true if write was successful.
     */
    bool writeCommand(const char *command);

    /**
     * @brief Issues a write of the given command n times.
     *
     * @return true if an OK response was returned for the written command.
     */
    bool retryCommand(const char *command, const uint8_t retries = 5);

    /**
     * @return -1 if failed to read or read value if else.
     */
    int16_t readByte(void);

    /**
     * @brief Reads a response after e.g. an AT command, will try to read until
     * an OK or ERROR (depending on the buffer size).
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
     *         - ERROR if read was successfull but result was terminated by
     * ERROR.
     *         - OVERFLOW if read resulted in buffer overflow.
     *         - SERIAL_READ_ERROR if an error occured in the serial interface
     */
    ResponseResult readResponse(char *out_buffer, uint16_t buffer_size);

    /**
     * @brief Reads the response without placing the content of the read
     * anywhere. Will return the same response as from readResponse(char
     * *out_buffer, uint16_t buffer_size).
     */
    ResponseResult readResponse(void);

    /**
     * @brief Will clear the receive buffer, will just set the ring buffer tail
     * and head indices to the same position, not issue any further reads.
     */
    void clearReceiveBuffer(void);

    /**
     * @brief Searches for a value at one index in the response, which has a
     * comma delimiter.
     *
     * @param response The AT command response.
     * @param index Index of value to extract.
     * @param buffer Destination buffer for value.
     * @param buffer_size Destination buffer size.
     * @param start_character Start character of the data or NULL if none.
     *
     * @return true if extraction was successful.
     */
    bool extractValueFromCommandResponse(
        char *response,
        const uint8_t index,
        char *buffer,
        const size_t buffer_size,
        const char start_character = URC_IDENTIFIER_END_CHARACTER);

    /**
     * @brief Registers for callbacks when an URC with the given identifier is
     * detected. There is a fixed amount of URC callbacks allowed for
     * registration.
     *
     * @return true if we didn't surpass the fixed amount of URC callbacks
     * allowed for registration.
     */
    bool registerCallback(const char *urc_identifier,
                          void (*urc_callback)(void));

    /**
     * @brief Unregister callback for a given URC identifier.
     */
    void unregisterCallback(const char *urc_identifier);

    /**
     * @brief Reads the latest URC/notification and places it into the buffer.
     *
     * @param buffer Where to place the URC.
     * @param buffer_size How many bytes to read. Buffer size for URC is
     * URC_DATA_BUFFER_SIZE bytes. If size is above that, no read will be
     * issued.
     *
     * @return false if the current URC already has been read or if buffer_size
     * is over URC_DATA_BUFFER_SIZE limit.
     */
    bool readNotification(char *buffer, uint8_t buffer_size);
};

extern SequansControllerClass SequansController;

#endif

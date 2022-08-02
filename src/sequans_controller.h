/**
 * @brief Interface for sending AT commands to and receiveing responses from the
 *        Sequans GM02S module. Singleton.
 */

#ifndef SEQUANS_CONTROLLER_H
#define SEQUANS_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Is public here for users of the interface
#define URC_DATA_BUFFER_SIZE 384

#define URC_IDENTIFIER_START_CHARACTER '+'
#define URC_IDENTIFIER_END_CHARACTER   ':'

#define SEQUANS_CONTROLLER_READ_BYTE_OK      1
#define SEQUANS_CONTROLLER_READ_BYTE_TIMEOUT 2

#define WAIT_FOR_URC_TIMEOUT_MS 20000

enum class ResponseResult {
    NONE = 0,
    OK,
    ERROR,
    BUFFER_OVERFLOW,
    TIMEOUT,
    SERIAL_READ_ERROR
};

class SequansControllerClass {

  private:
    /**
     * @brief Constructor is hidden to enforce a single instance of this class
     * through a singleton.
     */
    SequansControllerClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static SequansControllerClass& instance(void) {
        static SequansControllerClass instance;
        return instance;
    }

    /**
     * @brief Sets up the pins for TX, RX, RTS and CTS of the serial interface
     * towards the LTE module.
     */
    void begin(void);

    /**
     * @return True if begin has already been called.
     */
    bool isInitialized(void);

    /**
     * @brief Disables interrupts used for the sequans module and closes the
     * serial interface.
     */
    void end(void);

    /**
     * @return True if transmit buffer is not full.
     */
    bool isTxReady(void);

    /**
     * @return True if receive buffer is not empty.
     */
    bool isRxReady(void);

    /**
     * @brief Writes a data buffer to the modem.
     */
    void writeBytes(const uint8_t* data,
                    const size_t buffer_size,
                    const bool append_carriage_return = false);

    /**
     * @return -1 if failed to read or read value if else.
     */
    int16_t readByte(void);

    /**
     * @brief Will clear the receive buffer, will just set the ring buffer tail
     * and head indices to the same position, not issue any further reads.
     */
    void clearReceiveBuffer(void);

    /**
     * @brief Writes an AT command in the form of a string to the LTE module. If
     * a timeout occurs, the command will be sent again until timeout doesn't
     * occur.
     *
     * @note A carrige return is not needed for the command as it is appended.
     *
     * @param result_buffer Result will be placed in this buffer if not NULL.
     * @param result_buffer_size Size of the result buffer.
     *
     * @return The following status codes:
     * - OK if read was successfull and resultw as terminated by OK.
     * - ERROR if read was successfull but result was terminated by ERROR.
     * - OVERFLOW if read resulted in buffer overflow.
     * - SERIAL_READ_ERROR if an error occured in the serial interface.
     */
    ResponseResult writeCommand(const char* command,
                                char* result_buffer             = NULL,
                                const size_t result_buffer_size = 0);

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
     * @param out_buffer Buffer to place the response (if needed).
     * @param out_buffer_size Max size of response bytes to read (if needed).
     *
     * @return The following status codes:
     * - OK if read was successfull and resultw as terminated by OK.
     * - ERROR if read was successfull but result was terminated by ERROR.
     * - OVERFLOW if read resulted in buffer overflow.
     * - TIMEOUT if no response was received before timing out.
     * - SERIAL_READ_ERROR if an error occured in the serial interface.
     */
    ResponseResult readResponse(char* out_buffer             = NULL,
                                const size_t out_buffer_size = 0);

    /**
     * @brief Searches for a value at one index in the response, which has a
     * comma delimiter.
     *
     * @param response The AT command response.
     * @param index Index of value to extract.
     * @param buffer Destination buffer for value.
     * @param buffer_size Destination buffer size.
     * @param start_character Start character of the data or NULL if none.
     * Default is URC_IDENTIFIER_END_CHARACTER, which specifies the end of the
     * identifier and the start of data.
     *
     * @return true if extraction was successful.
     */
    bool extractValueFromCommandResponse(
        char* response,
        const uint8_t index,
        char* buffer,
        const size_t buffer_size,
        const char start_character = URC_IDENTIFIER_END_CHARACTER);

    /**
     * @brief Registers for callbacks when an URC with the given identifier is
     * detected. There is a fixed amount of URC callbacks allowed for
     * registration.
     *
     * @param urc_callback Callback with URC data as argument.
     * @param clear_data Whether the UART RX buffer will be cleared of the URC
     * or not. This is to save space in the buffer.
     *
     * @return true if we didn't surpass the fixed amount of URC callbacks
     * allowed for registration.
     */
    bool registerCallback(const char* urc_identifier,
                          void (*urc_callback)(char*),
                          const bool clear_data = true);

    /**
     * @brief Unregister callback for a given URC identifier.
     */
    void unregisterCallback(const char* urc_identifier);

    /**
     * @brief Waits for a given URC.
     *
     * @param urc_identifier The identifier of the URC.
     * @param out_buffer The data payload from the URC.
     * @param out_buffer_size Size of the output buffer for the URC.
     * @param timeout_ms How long the waiting period is.
     *
     * @return true if URC was retrieved before the timeout.
     */
    bool waitForURC(const char* urc_identifier,
                    char* out_buffer               = NULL,
                    const uint16_t out_buffer_size = URC_DATA_BUFFER_SIZE,
                    const uint64_t timeout_ms      = WAIT_FOR_URC_TIMEOUT_MS);

    /**
     * @brief Sets the power saving mode for the Sequans modem.
     *
     * @param mode 1 if ought to enter power save mode, 0 if not.
     * @param ring_callback Called when the ring line is asserted. Can be
     * used as a measure to figure out when the modem is in deep sleep.
     */
    void setPowerSaveMode(const uint8_t mode, void (*ring_callback)(void));

    /**
     * @brief Formats a string based on the @p response_result value and
     * places it in @p response_string. @p response_string has to be
     * pre-allocated.
     */
    void responseResultToString(const ResponseResult response_result,
                                char* response_string);

    /**
     * @brief Waits until @p timeout for the character specified by @p byte.
     *
     * @return True if character received within timeout, false if not.
     */
    bool waitForByte(const uint8_t byte, const uint32_t timeout_ms);

    /**
     * @brief Will assert the RTS line for the modem such that it will stop
     * sending data.
     */
    void startCriticalSection(void);

    /**
     * @brief Will de-assert the RTS line for the modem such that it can send
     * data again.
     */
    void stopCriticalSection(void);
};

extern SequansControllerClass SequansController;

#endif

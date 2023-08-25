/**
 * @brief Interface for sending AT commands to and receiveing responses from the
 *        Sequans GM02S module. Singleton.
 */

#ifndef SEQUANS_CONTROLLER_H
#define SEQUANS_CONTROLLER_H

#include <WString.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define URC_DATA_BUFFER_SIZE (384)

#define URC_IDENTIFIER_START_CHARACTER '+'
#define URC_IDENTIFIER_END_CHARACTER   ':'

#define WAIT_FOR_URC_TIMEOUT_MS (20000)

enum class ResponseResult {
    NONE = 0,
    OK,
    ERROR,
    BUFFER_OVERFLOW,
    TIMEOUT,
    SERIAL_READ_ERROR,
    SERIAL_WRITE_ERROR
};

class SequansControllerClass {

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
     *
     * @return True if the modem reported with the SYSSTAR URC.
     */
    bool begin(void);

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
     * @brief Will clear the receive buffer, will just set the ring buffer tail
     * and head indices to the same position, not issue any further reads.
     */
    void clearReceiveBuffer(void);

    /**
     * @return -1 if failed to read or read value if else.
     */
    int16_t readByte(void);

    /**
     * @brief Writes a data buffer to the modem. This does not check any
     * response from the modem (for that functionality, see #writeCommand).
     *
     * @return false if the modem is was not ready to accept all data.
     */
    bool writeBytes(const uint8_t* data,
                    const size_t buffer_size,
                    const bool append_carriage_return = false);
    /**
     * @brief Writes a string to the modem (with optinal formatting). This does
     * not check any response from the modem (for that functionality, see
     * #writeCommand).
     *
     * @return false if the modem is was not ready to accept all data.
     */
    bool writeString(const char* str,
                     const bool append_carriage_return = false,
                     ...);

    /**
     * @brief Flash string version of #writeString.
     */
    bool writeString(const __FlashStringHelper* str,
                     const bool append_carriage_return = false,
                     ...);

    /**
     * @brief Writes an AT command in the form of a string to the modem. The
     * command can be a formatted string. In that case, arguments has to be
     * passed for the formatting. If the command fails, it will retry
     * #COMMAND_NUM_RETRIES before giving up. The difference between this and
     * #writeBytes is the retry mechanism and the return value of a response.
     *
     * @note A carrige return is not needed for the command as it is appended.
     *
     * @param command The AT command to write.
     * @param result_buffer Result will be placed in this buffer if not NULL.
     * @param result_buffer_size Size of the result buffer.
     * @param ... Optional arguments for the command.
     *
     * @return The following status codes:
     * - OK if read was successfull and resultw as terminated by OK.
     * - ERROR if read was successfull but result was terminated by ERROR.
     * - OVERFLOW if read resulted in buffer overflow.
     * - SERIAL_READ_ERROR if an error occured in the serial interface.
     */
    ResponseResult writeCommand(const char* command,
                                char* result_buffer             = NULL,
                                const size_t result_buffer_size = 0,
                                ...);

    /**
     * @brief Flash string version of #writeCommand.
     */
    ResponseResult writeCommand(const __FlashStringHelper* command,
                                char* result_buffer             = NULL,
                                const size_t result_buffer_size = 0,
                                ...);

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
     * @param destination_buffer Destination buffer for value.
     * @param destination_buffer_size Destination buffer size.
     * @param start_character Start character of the data or NULL if none.
     * Default is URC_IDENTIFIER_END_CHARACTER, which specifies the end of the
     * identifier and the start of data.
     *
     * @return true if extraction was successful.
     */
    bool extractValueFromCommandResponse(
        char* response,
        const uint8_t index,
        char* destination_buffer,
        const size_t destination_buffer_size,
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
     * @brief Flash string version of #registerCallback.
     */
    bool registerCallback(const __FlashStringHelper* urc_identifier,
                          void (*urc_callback)(char*),
                          const bool clear_data = true);

    /**
     * @brief Unregister callback for a given URC identifier.
     */
    void unregisterCallback(const char* urc_identifier);

    /**
     * @brief Flash string version of #unregisterCallback.
     */
    void unregisterCallback(const __FlashStringHelper* urc_identifier);

    /**
     * @brief Waits for a given URC.
     *
     * @param urc_identifier The identifier of the URC.
     * @param out_buffer The data payload from the URC.
     * @param out_buffer_size Size of the output buffer for the URC.
     * @param timeout_ms How long the waiting period is.
     * @param action Action to do while waiting (blinking LED for example). The
     * action can be used to prematurely exit the waiting period (if it returns
     * false).
     * @param action_interval_ms Interval between calling @p action.
     *
     * @return true if URC was retrieved before the timeout or before the @p
     * action prematurely aborted the waiting.
     */
    bool waitForURC(const char* urc_identifier,
                    char* out_buffer                  = NULL,
                    const uint16_t out_buffer_size    = URC_DATA_BUFFER_SIZE,
                    const uint32_t timeout_ms         = WAIT_FOR_URC_TIMEOUT_MS,
                    void (*action)(void)              = NULL,
                    const uint32_t action_interval_ms = 0);

    /**
     * @brief Flash string version of #waitForURC.
     */
    bool waitForURC(const __FlashStringHelper* urc_identifier,
                    char* out_buffer                  = NULL,
                    const uint16_t out_buffer_size    = URC_DATA_BUFFER_SIZE,
                    const uint32_t timeout_ms         = WAIT_FOR_URC_TIMEOUT_MS,
                    void (*action)(void)              = NULL,
                    const uint32_t action_interval_ms = 0);

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

  private:
    /**
     * @brief Constructor is hidden to enforce a single instance of this class
     * through a singleton.
     */
    SequansControllerClass(){};

    /**
     * @brief See #writeString. This function is meant to be internal and the
     * #writeString functions call this with the additional flag for
     * whether the string is stored in program memory or not.
     */
    bool writeString(const char* str,
                     const bool append_carriage_return,
                     const bool is_flash_string,
                     va_list args);

    /**
     * @brief See #writeCommand. This function is meant to be internal and the
     * #writeCommand functions call this with the additional flag for
     * whether the command is stored in program memory or not.
     */
    ResponseResult writeCommand(const char* command,
                                char* result_buffer,
                                const size_t result_buffer_size,
                                const bool is_flash_string,
                                va_list args);

    /**
     * @brief See #registerCallback. This function is meant to be internal and
     * the #registerCallback functions call this with the additional flag for
     * whether the URC identifier is stored in program memory or not.
     */
    bool registerCallback(const char* urc_identifier,
                          void (*urc_callback)(char*),
                          const bool clear_data,
                          const bool is_flash_string);
    /**
     * @brief See #unregisterCallback. This function is meant to be internal and
     * the #unregisterCallback functions call this with the additional flag for
     * whether the URC identifier is stored in program memory or not.
     */
    void unregisterCallback(const char* urc_identifier,
                            const bool is_flash_string);

    /**
     * @brief See #waitForURC. This function is meant to be internal and the
     * #waitForURC functions call this with the additional flag for whether the
     * URC identifier is stored in program memory or not.
     */
    bool waitForURC(const char* urc_identifier,
                    char* out_buffer,
                    const uint16_t out_buffer_size,
                    const uint32_t timeout_ms,
                    void (*action)(void),
                    const uint32_t action_interval_ms,
                    const bool is_flash_string);
};

extern SequansControllerClass SequansController;

#endif

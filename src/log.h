#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

enum class LogLevel { NONE = 0, ERROR, WARN, INFO, DEBUG };

class LogClass {
  public:
    /**
     * @brief Constructs the LogClass with a reference to the UART Arduino
     * driver.
     *
     * @param uart [in] The UART Arduino driver.
     */
    LogClass(UartClass* uart);

    /**
     * @brief Set the UART Arduino driver. Used for swapping between different
     * UARTs during run-time.
     *
     * @param uart [in] The UART Arduino driver.
     */
    void setOutputUart(UartClass* uart);

    /**
     * @brief Set the log level based on the #LogLevel enumeration.
     *
     * @param log_level [in] The log level.
     */
    void setLogLevel(const LogLevel log_level);

    /**
     * @return The current log level.
     */
    LogLevel getLogLevel();

    /**
     * @brief Set the log level based on a string.
     *
     * @param log_level [in] The log level as a string (lower case).
     *
     * @return true If the string matches a defined log level.
     */
    bool setLogLevelStr(const char* log_level);

    /**
     * @brief Starts the underlying Arduino UART driver with the given @p
     * baud_rate.
     *
     * @param baud_rate [in] Baud rate to use for the UART driver.
     */
    void begin(const uint32_t baud_rate);

    /**
     * @brief Ends the underlying Arduino UART driver.
     */
    void end();

    /**
     * @brief Outputs a string with the ERROR log level.
     *
     * @param str [in] String to output.
     */
    void error(const char str[]);

    /**
     * @brief Outputs a string with the error log level.
     *
     * @param str [in] String to output.
     */
    void error(const String str);

    /**
     * @brief Outputs a string with the error log level, where the string is
     * stored in program memory.
     *
     * @param str [in] String to output.
     */
    void error(const __FlashStringHelper* str);

    /**
     * @brief Outputs a string with the error log level and optional formatting.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void errorf(const char* format, ...);

    /**
     * @brief Outputs a string with the error log level and optional formatting,
     * where the formatting is stored in program memory.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void errorf(const __FlashStringHelper* format, ...);

    /**
     * @brief Outputs a string with the warning log level.
     *
     * @param str [in] String to output.
     */
    void warn(const char str[]);

    /**
     * @brief Outputs a string with the warning log level.
     *
     * @param str [in] String to output.
     */
    void warn(const String str);

    /**
     * @brief Outputs a string with the warning log level, where the string is
     * stored in program memory.
     *
     * @param str [in] String to output.
     */
    void warn(const __FlashStringHelper* str);

    /**
     * @brief Outputs a string with the warning log level and optional
     * formatting.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void warnf(const char* format, ...);

    /**
     * @brief Outputs a string with the warning log level and optional
     * formatting, where the formatting is stored in program memory.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void warnf(const __FlashStringHelper* format, ...);

    /**
     * @brief Outputs a string with the info log level.
     *
     * @param str [in] String to output.
     */
    void info(const char str[]);

    /**
     * @brief Outputs a string with the info log level.
     *
     * @param str [in] String to output.
     */
    void info(const String str);

    /**
     * @brief Outputs a string with the info log level, where the string is
     * stored in program memory.
     *
     * @param str [in] String to output.
     */
    void info(const __FlashStringHelper* str);

    /**
     * @brief Outputs a string with the info log level and optional formatting.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void infof(const char* format, ...);

    /**
     * @brief Outputs a string with the info log level and optional formatting,
     * where the formatting is stored in program memory.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void infof(const __FlashStringHelper* format, ...);

    /**
     * @brief Outputs a string with the debug log level.
     *
     * @param str [in] String to output.
     */
    void debug(const char str[]);

    /**
     * @brief Outputs a string with the debug log level.
     *
     * @param str [in] String to output.
     */
    void debug(const String str);

    /**
     * @brief Outputs a string with the debug log level, where the string is
     * stored in program memory.
     *
     * @param str [in] String to output.
     */
    void debug(const __FlashStringHelper* str);

    /**
     * @brief Outputs a string with the debug log level and optional formatting.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void debugf(const char* format, ...);

    /**
     * @brief Outputs a string with the debug log level and optional formatting,
     * where the formatting is stored in program memory.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void debugf(const __FlashStringHelper* format, ...);

    /**
     * @brief Outputs a string without log level..
     *
     * @param str [in] String to output.
     */
    void raw(const char str[]);

    /**
     * @brief Outputs a string without log level.
     *
     * @param str [in] String to output.
     */
    void raw(const String str);

    /**
     * @brief Outputs a string without log level, where the string is stored in
     * program memory.
     *
     * @param str [in] String to output.
     */
    void raw(const __FlashStringHelper* str);

    /**
     * @brief Outputs a string without log level and optional formatting.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void rawf(const char* format, ...);

    /**
     * @brief Outputs a string without log level and optional formatting, where
     * the formatting is stored in program memory.
     *
     * @param format [in] Format string.
     * @param ... [in] Arguments for formatting.
     */
    void rawf(const __FlashStringHelper* format, ...);

  private:
    /**
     * @brief Reference to the UART Arduino driver.
     */
    UartClass* uart;

    /**
     * @brief Stores the current #LogLevel.
     */
    LogLevel log_level;

    /**
     * @brief Prints a string without formatting and a level format. If the
     * current log level set is lower than the requested @p level, nothing is
     * printed.
     *
     * @param level [in] Log level (DEBUG, INFO, WARN, ERROR).
     * @param str [in] String to print.
     */
    void print(const LogLevel level, const char* str);

    /**
     * @brief Prints a string stored in flash without formatting and a level
     * format. If the current log level set is lower than the requested @p
     * level, nothing is printed.
     *
     * @param level [in] Log level (DEBUG, INFO, WARN, ERROR).
     * @param str [in] String to print.
     */
    void print(const LogLevel level, const __FlashStringHelper* str);

    /**
     * @brief Prints a string with formatting and a level format. If the
     * current log level set is lower than the requested @p level, nothing is
     * printed.
     *
     * @param level [in] Log level (DEBUG, INFO, WARN, ERROR).
     * @param str [in] String to print.
     * @param args [in] Arguments for formatting.
     */
    void printf(const LogLevel level, const char* str, va_list args);

    /**
     * @brief Prints a string stored in flash with formatting and a level
     * format. If the current log level set is lower than the requested @p
     * level, nothing is printed.
     *
     * @param level [in] Log level (DEBUG, INFO, WARN, ERROR).
     * @param str [in] String to print.
     * @param args [in] Arguments for formatting.
     */
    void
    printf(const LogLevel level, const __FlashStringHelper* str, va_list args);
};

extern LogClass Log;

#endif

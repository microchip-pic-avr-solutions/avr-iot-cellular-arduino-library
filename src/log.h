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
     * In the following level functions, we explicitly use the postfix of v to
     * specify that we pass on a va_list. In this way, the compiler is not
     * confused between calling e.g. infof with a va_list and calling infofv (as
     * we should if we're passing on a va_list).
     */

    void error(const char str[]);
    void error(const String str);
    void error(const __FlashStringHelper* str);
    void errorf(const char* format, ...);
    void errorfv(const char* format, va_list args);
    void errorf(const __FlashStringHelper* format, ...);
    void errorfv(const __FlashStringHelper* format, va_list args);

    void warn(const char str[]);
    void warn(const String str);
    void warn(const __FlashStringHelper* str);
    void warnf(const char* format, ...);
    void warnfv(const char* format, va_list args);
    void warnf(const __FlashStringHelper* format, ...);
    void warnfv(const __FlashStringHelper* format, va_list args);

    void info(const char str[]);
    void info(const String str);
    void info(const __FlashStringHelper* str);
    void infof(const char* format, ...);
    void infofv(const char* format, va_list args);
    void infof(const __FlashStringHelper* format, ...);
    void infofv(const __FlashStringHelper* format, va_list args);

    void debug(const char str[]);
    void debug(const String str);
    void debug(const __FlashStringHelper* str);
    void debugf(const char* format, ...);
    void debugfv(const char* format, va_list args);
    void debugf(const __FlashStringHelper* format, ...);
    void debugfv(const __FlashStringHelper* format, va_list args);

    void raw(const char str[]);
    void raw(const String str);
    void raw(const __FlashStringHelper* str);
    void rawf(const char* format, ...);
    void rawfv(const char* format, va_list args);
    void rawf(const __FlashStringHelper* format, ...);
    void rawfv(const __FlashStringHelper* format, va_list args);

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

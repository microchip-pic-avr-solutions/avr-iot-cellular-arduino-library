#include "log.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvarargs"

static const char ERROR_LOG_LEVEL[] PROGMEM = "[ERROR] ";
static const char WARN_LOG_LEVEL[] PROGMEM  = "[WARN] ";
static const char INFO_LOG_LEVEL[] PROGMEM  = "[INFO] ";
static const char DEBUG_LOG_LEVEL[] PROGMEM = "[DEBUG] ";
static const char NONE_LOG_LEVEL[] PROGMEM  = "";

/**
 * @return A pointer to the given @p level flash string.
 */
static const __FlashStringHelper* getLogLevelString(const LogLevel level) {

    switch (level) {
    case LogLevel::ERROR:
        return reinterpret_cast<const __FlashStringHelper*>(ERROR_LOG_LEVEL);

    case LogLevel::WARN:
        return reinterpret_cast<const __FlashStringHelper*>(WARN_LOG_LEVEL);

    case LogLevel::INFO:
        return reinterpret_cast<const __FlashStringHelper*>(INFO_LOG_LEVEL);

    case LogLevel::DEBUG:
        return reinterpret_cast<const __FlashStringHelper*>(DEBUG_LOG_LEVEL);

    default:
        return reinterpret_cast<const __FlashStringHelper*>(NONE_LOG_LEVEL);
    }
}

/**
 * @brief We have to declare this outside the class scope for interoperability
 * with fdev_setup_stream. This is safe as we won't ever come into a situation
 * where this is not called atomically.
 */
static int16_t fdev_putchar(char data, FILE* file) {
    ((class Print*)(fdev_get_udata(file)))->write((uint8_t)data);
    return 0;
}

/**
 * @brief The instance of the LogClass defined in the header file. Declared here
 * so that we have an Arduino style with not having to instantiate the class
 * explicitly.
 */
LogClass Log(&Serial3);

void LogClass::print(const LogLevel level, const char* str) {
    if (log_level < level) {
        return;
    }

    uart->print(getLogLevelString(level));
    uart->println(str);
}

void LogClass::print(const LogLevel level, const __FlashStringHelper* str) {
    if (log_level < level) {
        return;
    }

    uart->print(getLogLevelString(level));
    uart->println(str);
}

void LogClass::printf(const LogLevel level, const char* str, va_list args) {

    if (log_level < level) {
        return;
    }

    uart->print(getLogLevelString(level));

    FILE file;

    fdev_setup_stream(&file, fdev_putchar, NULL, _FDEV_SETUP_WRITE);
    fdev_set_udata(&file, this->uart);
    vfprintf(&file, str, args);
    fdev_close();
}

void LogClass::printf(const LogLevel level,
                      const __FlashStringHelper* str,
                      va_list args) {

    if (log_level < level) {
        return;
    }

    uart->print(getLogLevelString(level));

    FILE file;

    fdev_setup_stream(&file, fdev_putchar, NULL, _FDEV_SETUP_WRITE);
    fdev_set_udata(&file, this->uart);
    vfprintf_P(&file, reinterpret_cast<const char*>(str), args);
    fdev_close();
}

LogClass::LogClass(UartClass* uart) {
    this->uart = uart;
    log_level  = LogLevel::INFO;
}

void LogClass::setOutputUart(UartClass* uart) { this->uart = uart; }

void LogClass::setLogLevel(const LogLevel log_level) {
    this->log_level = log_level;
}

LogLevel LogClass::getLogLevel(void) { return log_level; }

bool LogClass::setLogLevelStr(const char* log_level) {
    LogLevel ll = LogLevel::NONE;
    if (strstr_P(log_level, PSTR("debug")) != NULL) {
        ll = LogLevel::DEBUG;
    } else if (strstr_P(log_level, PSTR("info")) != NULL) {
        ll = LogLevel::INFO;
    } else if (strstr_P(log_level, PSTR("warn")) != NULL) {
        ll = LogLevel::WARN;
    } else if (strstr_P(log_level, PSTR("error")) != NULL) {
        ll = LogLevel::ERROR;
    } else {
        return false;
    }

    this->setLogLevel(ll);

    return true;
}

void LogClass::begin(const uint32_t baud_rate) { this->uart->begin(baud_rate); }

void LogClass::end(void) { this->uart->end(); }

void LogClass::error(const char str[]) { this->print(LogLevel::ERROR, str); }

void LogClass::error(const String str) {
    this->print(LogLevel::ERROR, str.c_str());
}

void LogClass::error(const __FlashStringHelper* str) {
    this->print(LogLevel::ERROR, str);
}

void LogClass::errorf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::ERROR, format, args);
    va_end(args);
}

void LogClass::errorfv(const char* format, va_list args) {
    this->printf(LogLevel::ERROR, format, args);
}

void LogClass::errorf(const __FlashStringHelper* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::ERROR, format, args);
    va_end(args);
}

void LogClass::errorfv(const __FlashStringHelper* format, va_list args) {
    this->printf(LogLevel::ERROR, format, args);
}

void LogClass::warn(const char str[]) { this->print(LogLevel::WARN, str); }

void LogClass::warn(const String str) {
    this->print(LogLevel::WARN, str.c_str());
}

void LogClass::warn(const __FlashStringHelper* str) {
    this->print(LogLevel::WARN, str);
}

void LogClass::warnf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::WARN, format, args);
    va_end(args);
}

void LogClass::warnfv(const char* format, va_list args) {
    this->printf(LogLevel::WARN, format, args);
}

void LogClass::warnf(const __FlashStringHelper* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::WARN, format, args);
    va_end(args);
}

void LogClass::warnfv(const __FlashStringHelper* format, va_list args) {
    this->printf(LogLevel::WARN, format, args);
}

void LogClass::info(const char str[]) { this->print(LogLevel::INFO, str); }

void LogClass::info(const String str) {
    this->print(LogLevel::INFO, str.c_str());
}

void LogClass::info(const __FlashStringHelper* str) {
    this->print(LogLevel::INFO, str);
}

void LogClass::infof(const char* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::INFO, format, args);
    va_end(args);
}

void LogClass::infofv(const char* format, va_list args) {
    this->printf(LogLevel::INFO, format, args);
}

void LogClass::infof(const __FlashStringHelper* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::INFO, format, args);
    va_end(args);
}

void LogClass::infofv(const __FlashStringHelper* format, va_list args) {
    this->printf(LogLevel::INFO, format, args);
}

void LogClass::debug(const char str[]) { this->print(LogLevel::DEBUG, str); }

void LogClass::debug(const String str) {
    this->print(LogLevel::DEBUG, str.c_str());
}

void LogClass::debug(const __FlashStringHelper* str) {
    this->print(LogLevel::DEBUG, str);
}

void LogClass::debugf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::DEBUG, format, args);
    va_end(args);
}

void LogClass::debugfv(const char* format, va_list args) {
    this->printf(LogLevel::DEBUG, format, args);
}

void LogClass::debugf(const __FlashStringHelper* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::DEBUG, format, args);
    va_end(args);
}

void LogClass::debugfv(const __FlashStringHelper* format, va_list args) {
    this->printf(LogLevel::DEBUG, format, args);
}

void LogClass::raw(const char str[]) { this->print(LogLevel::NONE, str); }

void LogClass::raw(const String str) {
    this->print(LogLevel::NONE, str.c_str());
}

void LogClass::raw(const __FlashStringHelper* str) {
    this->print(LogLevel::NONE, str);
}

void LogClass::rawf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::NONE, format, args);
    va_end(args);
}

void LogClass::rawfv(const char* format, va_list args) {
    this->printf(LogLevel::NONE, format, args);
}

void LogClass::rawf(const __FlashStringHelper* format, ...) {
    va_list args;
    va_start(args, format);
    this->printf(LogLevel::NONE, format, args);
    va_end(args);
}

void LogClass::rawfv(const __FlashStringHelper* format, va_list args) {
    this->printf(LogLevel::NONE, format, args);
}

#pragma GCC diagnostic pop

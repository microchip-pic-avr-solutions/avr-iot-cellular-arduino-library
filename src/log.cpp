#include "log.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvarargs"

#define ERR_LEVEL_FMT   "[ERROR] "
#define INFO_LEVEL_FMT  "[INFO] "
#define DEBUG_LEVEL_FMT "[DEBUG] "
#define WARN_LEVEL_FMT  "[WARN] "

#ifdef __AVR_AVR128DB48__ // MINI
LogClass Log(&Serial3);
#else
#ifdef __AVR_AVR128DB64__ // NON-MINI
LogClass Log(&Serial5);
#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

int16_t printf_putchar(char c, FILE *fp) {
    ((class Print *)(fdev_get_udata(fp)))->write((uint8_t)c);
    return 0;
}

void LogClass::print(const char *str, const char level[]) {
    this->uart->printf("%s%s\r\n", level, str);
}

LogClass::LogClass(UartClass *uart) {
    this->uart = uart;
    log_level = LogLevel::INFO;
}

void LogClass::setOutputUart(UartClass *uart) { this->uart = uart; }

void LogClass::setLogLevel(const LogLevel log_level) {
    this->log_level = log_level;
}

LogLevel LogClass::getLogLevel(void) { return log_level; }

bool LogClass::setLogLevelStr(const char *log_level) {
    LogLevel ll = LogLevel::NONE;
    if (strstr(log_level, "debug") != NULL) {
        ll = LogLevel::DEBUG;
    } else if (strstr(log_level, "info") != NULL) {
        ll = LogLevel::INFO;
    } else if (strstr(log_level, "warn") != NULL) {
        ll = LogLevel::WARN;
    } else if (strstr(log_level, "error") != NULL) {
        ll = LogLevel::ERROR;
    }

    if (ll == LogLevel::NONE) {
        return false;
    }

    this->setLogLevel(ll);
    return true;
}

void LogClass::begin(const uint32_t baud_rate) { this->uart->begin(baud_rate); }

void LogClass::end(void) { this->uart->end(); }

void LogClass::info(const char str[]) {
    if (log_level >= LogLevel::INFO) {
        this->print(str, INFO_LEVEL_FMT);
    }
}

void LogClass::info(const String str) { this->info(str.c_str()); }

void LogClass::infof(const char *format, ...) {
    if (log_level >= LogLevel::INFO) {

        // Append format with [ERROR]
        char nFormat[strlen(format) + sizeof(INFO_LEVEL_FMT)] = INFO_LEVEL_FMT;
        strcpy(nFormat + sizeof(INFO_LEVEL_FMT) - 1, format);

        FILE f;
        va_list ap;

        fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
        fdev_set_udata(&f, this->uart);

        va_start(ap, nFormat);
        vfprintf(&f, nFormat, ap);
    }
}

void LogClass::debug(const char str[]) {
    if (log_level >= LogLevel::DEBUG) {
        this->print(str, DEBUG_LEVEL_FMT);
    }
}

void LogClass::debug(const String str) { this->debug(str.c_str()); }

void LogClass::debugf(const char *format, ...) {
    if (log_level >= LogLevel::DEBUG) {

        // Append format with [ERROR]
        char nFormat[strlen(format) + sizeof(DEBUG_LEVEL_FMT)] =
            DEBUG_LEVEL_FMT;
        strcpy(nFormat + sizeof(DEBUG_LEVEL_FMT) - 1, format);

        FILE f;
        va_list ap;

        fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
        fdev_set_udata(&f, this->uart);
        va_start(ap, nFormat);
        vfprintf(&f, nFormat, ap);
    }
}

void LogClass::raw(const char str[]) {
    if (log_level >= LogLevel::DEBUG) {
        this->print(str, DEBUG_LEVEL_FMT);
    }

    this->uart->print(str);
}

void LogClass::raw(const String str) { this->raw(str.c_str()); }

void LogClass::rawf(const char *format, ...) {
    FILE f;
    va_list ap;

    fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
    fdev_set_udata(&f, this->uart);
    va_start(ap, format);
    vfprintf(&f, format, ap);
}

void LogClass::error(const char str[]) {
    if (log_level >= LogLevel::ERROR) {
        this->print(str, ERR_LEVEL_FMT);
    }
}

void LogClass::error(const String str) { this->error(str.c_str()); }

void LogClass::errorf(const char *format, ...) {
    if (log_level >= LogLevel::ERROR) {

        // Append format with [ERROR]
        char nFormat[strlen(format) + sizeof(ERR_LEVEL_FMT)] = ERR_LEVEL_FMT;
        strcpy(nFormat + sizeof(ERR_LEVEL_FMT) - 1, format);

        FILE f;
        va_list ap;

        fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
        fdev_set_udata(&f, this->uart);
        va_start(ap, nFormat);
        vfprintf(&f, nFormat, ap);
    }
}

void LogClass::warn(const char str[]) {
    if (log_level >= LogLevel::WARN) {
        this->print(str, WARN_LEVEL_FMT);
    }
}

void LogClass::warn(const String str) { this->warn(str.c_str()); }

void LogClass::warnf(const char *format, ...) {
    if (log_level >= LogLevel::WARN) {

        // Append format with [ERROR]
        char nFormat[strlen(format) + sizeof(WARN_LEVEL_FMT)] = WARN_LEVEL_FMT;
        strcpy(nFormat + sizeof(WARN_LEVEL_FMT) - 1, format);

        FILE f;
        va_list ap;

        fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
        fdev_set_udata(&f, this->uart);
        va_start(ap, nFormat);
        vfprintf(&f, nFormat, ap);
    }
}

#pragma GCC diagnostic pop

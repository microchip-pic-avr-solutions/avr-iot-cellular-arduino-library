#include "log.h"
#include <stdarg.h>
#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvarargs"

#define ERR_LEVEL_FMT   "[ERROR] "
#define INFO_LEVEL_FMT  "[INFO] "
#define DEBUG_LEVEL_FMT "[DEBUG] "
#define WARN_LEVEL_FMT  "[WARN] "

#ifdef HAVE_LOG
LogClass Log(&Serial5);
#endif

int16_t printf_putchar(char c, FILE *fp) {
    ((class Print *)(fdev_get_udata(fp)))->write((uint8_t)c);
    return 0;
}

template <typename T> void LogClass::print(const T str, const char level[]) {
    this->uart->printf("%s%s\n", level, str);
}

LogClass::LogClass(UartClass *uart) { this->uart = uart; }

void LogClass::setOutputUart(UartClass *uart) { this->uart = uart; }

void LogClass::setLogLevel(const LogLevel log_level) {
    this->log_level = log_level;
}

void LogClass::info(const char str[]) {
    if (log_level >= LogLevel::INFO) {
        this->print(str, INFO_LEVEL_FMT);
    }
}

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

void LogClass::error(const char str[]) {
    if (log_level >= LogLevel::ERROR) {
        this->print(str, ERR_LEVEL_FMT);
    }
}

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

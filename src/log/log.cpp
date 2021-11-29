#include "log.h"
#include <stdarg.h>
#include <stdio.h>

#define __ERR_LEVEL_FMT "[ERROR]"
#define __INFO_LEVEL_FMT "[INFO]"
#define __DEBUG_LEVEL_FMT "[DEBUG]"
#define __WARN_LEVEL_FMT "[WARN]"

#ifdef HAVE_LOG5
Log Log5(&Serial5);
#endif

int16_t printf_putchar(char c, FILE *fp)
{
	((class Print *)(fdev_get_udata(fp)))->write((uint8_t)c);
	return 0;
}

Log::Log(UartClass *uart)
{
	this->lUart = uart;
}

void Log::setLogLevel(LogLevels level)
{
	this->lLogLevel = level;
}

void Log::setOutputUart(UartClass *uart)
{
	this->lUart = uart;
}

template <typename T>
void Log::print(const T str, const char level[])
{
	this->lUart->printf("%s %s\n", level, str);
}

void Log::Info(const char str[])
{
	if (lLogLevel >= LogLevels::INFO)
	{
		this->print(str, __INFO_LEVEL_FMT);
	}
}

void Log::Info(const String &s)
{
	if (lLogLevel >= LogLevels::INFO)
	{
		this->print(s.c_str(), __INFO_LEVEL_FMT);
	}
}

void Log::Infof(const char *format, ...)
{
	if (lLogLevel >= LogLevels::INFO)
	{

		// Append format with [ERROR]
		char nFormat[strlen(format) + sizeof(__INFO_LEVEL_FMT)] = __INFO_LEVEL_FMT;
		strcpy(nFormat + sizeof(__INFO_LEVEL_FMT) - 1, format);

		FILE f;
		va_list ap;

		fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
		fdev_set_udata(&f, this->lUart);
		va_start(ap, nFormat);
		vfprintf(&f, nFormat, ap);
	}
}

void Log::Debug(const char str[])
{
	if (lLogLevel >= LogLevels::DEBUG)
	{
		this->print(str, __DEBUG_LEVEL_FMT);
	}
}

void Log::Debugf(const char *format, ...)
{
	if (lLogLevel >= LogLevels::DEBUG)
	{

		// Append format with [ERROR]
		char nFormat[strlen(format) + sizeof(__DEBUG_LEVEL_FMT)] = __DEBUG_LEVEL_FMT;
		strcpy(nFormat + sizeof(__DEBUG_LEVEL_FMT) - 1, format);

		FILE f;
		va_list ap;

		fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
		fdev_set_udata(&f, this->lUart);
		va_start(ap, nFormat);
		vfprintf(&f, nFormat, ap);
	}
}

void Log::Error(const char str[])
{
	if (lLogLevel >= LogLevels::ERROR)
	{
		this->print(str, __ERR_LEVEL_FMT);
	}
}

void Log::Errorf(const char *format, ...)
{
	if (lLogLevel >= LogLevels::ERROR)
	{

		// Append format with [ERROR]
		char nFormat[strlen(format) + sizeof(__ERR_LEVEL_FMT)] = __ERR_LEVEL_FMT;
		strcpy(nFormat + sizeof(__ERR_LEVEL_FMT) - 1, format);

		FILE f;
		va_list ap;

		fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
		fdev_set_udata(&f, this->lUart);
		va_start(ap, nFormat);
		vfprintf(&f, nFormat, ap);
	}
}

void Log::Warn(const char str[])
{
	if (lLogLevel >= LogLevels::WARN)
	{
		this->print(str, __WARN_LEVEL_FMT);
	}
}

void Log::Warnf(const char *format, ...)
{
	if (lLogLevel >= LogLevels::WARN)
	{

		// Append format with [ERROR]
		char nFormat[strlen(format) + sizeof(__WARN_LEVEL_FMT)] = __WARN_LEVEL_FMT;
		strcpy(nFormat + sizeof(__WARN_LEVEL_FMT) - 1, format);

		FILE f;
		va_list ap;

		fdev_setup_stream(&f, printf_putchar, NULL, _FDEV_SETUP_WRITE);
		fdev_set_udata(&f, this->lUart);
		va_start(ap, nFormat);
		vfprintf(&f, nFormat, ap);
	}
}
#pragma once

#include <Arduino.h>

enum class LogLevels
{
	NONE = 0,
	ERROR,
	WARN,
	INFO,
	DEBUG
};

class Log
{
private:
	template <typename T>
	void print(const T str, const char level[]);

	UartClass *lUart;
	LogLevels lLogLevel;

public:
	Log(UartClass *uart);

	void begin(unsigned long baud);

	void setOutputUart(UartClass *uart);
	void setLogLevel(LogLevels level);

	void Error(const char str[]);
	void Errorf(const char *format, ...);

	void Warn(const char str[]);
	void Warnf(const char *format, ...);

	void Info(const char str[]);
	void Info(const String &s);

	void Infof(const char *format, ...);

	void Debug(const char str[]);
	void Debugf(const char *format, ...);

	void Raw(const char str[]);
	void Rawf(const char *format, ...);
};

#if defined(USART5)
extern Log Log5;
#define HAVE_LOG5
#endif

#if defined(USART3)
extern Log Log3;
#define HAVE_LOG3
#endif

#ifdef __AVR_AVR128DB48__
#define LOG Log3
#else
#ifdef __AVR_AVR128DB64__
#define LOG Log5
#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

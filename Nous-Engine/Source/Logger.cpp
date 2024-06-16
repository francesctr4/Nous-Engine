#include "Logger.h"
#include "Asserts.h"

#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>

void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line) 
{
	LogOutput(LogLevel::LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message, file, line);
}

bool InitializeLogging()
{
	return false;
}

void ShutdownLogging()
{

}

void LogOutput(LogLevel level, const char* message, ...)
{
	const char* levelStrings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: "};

	char out_message[32000];
	memset(out_message, 0, sizeof(out_message));

	va_list arg_ptr;
	va_start(arg_ptr, message);
	vsnprintf_s(out_message, 32000, message, arg_ptr);
	va_end(arg_ptr);

	char out_message2[32000];
	sprintf_s(out_message2, "%s%s\n", levelStrings[(int)level], out_message);

	printf_s("%s", out_message2);
}
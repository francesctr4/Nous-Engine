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
    const char* levelStrings[6] = { "[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: " };

    // Calculate the size needed for the formatted message
    va_list arg_ptr;
    va_start(arg_ptr, message);
    int message_len = vsnprintf(NULL, 0, message, arg_ptr);
    va_end(arg_ptr);

    NOUS_ASSERT_MSG(message_len >= 0, "vsnprintf failed to calculate the message length");

    // Allocate memory for the formatted message
    char* out_message = (char*)malloc(message_len + 1);
    NOUS_ASSERT_MSG(out_message != NULL, "Memory allocation for out_message failed");

    va_start(arg_ptr, message);
    vsnprintf(out_message, message_len + 1, message, arg_ptr);
    va_end(arg_ptr);

    // Calculate the total size needed for the final message
    int level_len = strlen(levelStrings[level]);
    int total_len = level_len + message_len + 2; // +2 for the newline and null terminator

    // Allocate memory for the final message
    char* out_message2 = (char*)malloc(total_len);
    NOUS_ASSERT_MSG(out_message2 != NULL, "Memory allocation for out_message2 failed");

    // Construct the final message
    snprintf(out_message2, total_len, "%s%s\n", levelStrings[level], out_message);

    // Print the final message
    printf("%s", out_message2);

    // Free allocated memory
    free(out_message);
    free(out_message2);
}
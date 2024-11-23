#include "Logger.h"
#include "Asserts.h"
#include "FileHandle.h"

#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>

static FileHandle logFile;

void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line) 
{
	LogOutput(LogLevel::LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message, file, line);
}

// Platform-Specific Windows
void PrintToConsoleColor(const char* message, WORD color) {

    // Get the handle to the console
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // Save the current text color
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD savedAttributes = consoleInfo.wAttributes;

    // Set the text color
    SetConsoleTextAttribute(hConsole, color);

    // Print the message
    printf("%s", message);

    // Restore the original text color
    SetConsoleTextAttribute(hConsole, savedAttributes);
}

bool InitializeLogging()
{
    // Create new/wipe existing log file, then open it.
    if (!logFile.Open("console.log", FileMode::WRITE, false))
    {
        NOUS_ERROR("Unable to open console.log for writing.");
        return false;
    }

	return true;
}

void ShutdownLogging()
{
    logFile.Close();
}

void AppendToLogFile(const char* message)
{
    if (logFile.IsOpen())
    {
        uint64 length = static_cast<uint64>(std::strlen(message));
        uint64 written = 0;

        if (!logFile.Write(length, message, &written)) 
        {
            NOUS_ERROR("Error writing to console.log.");
        }
    }
}

void LogOutput(LogLevel level, const char* message, ...)
{
    // TODO: These string operations are all pretty slow. This needs to be
    // moved to another thread eventually, along with the file writes, to
    // avoid slowing things down while the engine is trying to run.

    const char* levelStrings[6] = { "[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: " };
    uint8_t levelColor[6] = { 64, 4, 6, 2, 1, 8 };

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
    size_t level_len = strlen(levelStrings[level]);
    size_t total_len = level_len + message_len + 2; // +2 for the newline and null terminator

    // Allocate memory for the final message
    char* out_message2 = (char*)malloc(total_len);
    NOUS_ASSERT_MSG(out_message2 != NULL, "Memory allocation for out_message2 failed");

    // Construct the final message
    snprintf(out_message2, total_len, "%s%s\n", levelStrings[level], out_message);

    // Print the final message
    PrintToConsoleColor(out_message2, levelColor[level]);

    // Queue a copy to be written to the log file.
    AppendToLogFile(out_message2);

    // Free allocated memory
    free(out_message);
    free(out_message2);
}
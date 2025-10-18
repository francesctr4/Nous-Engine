#include "Logger.h"
#include "Engine/Utils/Asserts.h"
#include "Engine/Systems/File System/FileHandle.h"
#include "Engine/Systems/Time Management/TimeManager.h"

#include <cstdio>
#include <deque>
#include <cstring>
#include <cstdarg>
#include <functional>
#include <mutex>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif

static std::atomic<bool> loggingPaused = false;

// Enabled log levels (bitmask)
static bool logLevelEnabled[LOG_LEVEL_MAX] = {
        true, true, true, true, true, false // Default: all enabled
};

static FileHandle logFile;
static std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> logHistory;
static const size_t MAX_LOG_HISTORY = 10000;

static std::function<void(LogLevel, LogChannel, double, const char*)> logCallback = nullptr;
static std::mutex logMutex; // protect log history (in case of multithreaded logging)

void SetLogCallback(std::function<void(LogLevel, LogChannel, double, const char*)> callback) {
    std::scoped_lock lock(logMutex);
    logCallback = std::move(callback);
}

std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> GetLogHistory()
{
    std::scoped_lock lock(logMutex);
    return logHistory; // ✅ safe copy
}

void ClearLogHistory() {
    std::scoped_lock lock(logMutex);
    logHistory.clear();
}

void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line) {
    LogOutput(LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d",
              expression, message, file, line);
}

// Platform console color printing
void PrintToConsoleColor(const char* message, uint8_t color) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD savedAttributes = consoleInfo.wAttributes;

    WORD windowsColor;
    switch (color) {
        case 64: windowsColor = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case 4:  windowsColor = FOREGROUND_RED; break;
        case 6:  windowsColor = FOREGROUND_RED | FOREGROUND_GREEN; break;
        case 2:  windowsColor = FOREGROUND_GREEN; break;
        case 1:  windowsColor = FOREGROUND_BLUE; break;
        case 8:  windowsColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        default: windowsColor = savedAttributes; break;
    }

    SetConsoleTextAttribute(hConsole, windowsColor);
    printf("%s", message);
    SetConsoleTextAttribute(hConsole, savedAttributes);

#elif defined(__linux__) || defined(__APPLE__)
    const char* ansiColor;
    switch (color) {
        case 64: ansiColor = "\033[1;31m"; break;
        case 4:  ansiColor = "\033[0;31m"; break;
        case 6:  ansiColor = "\033[0;33m"; break;
        case 2:  ansiColor = "\033[0;32m"; break;
        case 1:  ansiColor = "\033[0;34m"; break;
        case 8:  ansiColor = "\033[0;37m"; break;
        default: ansiColor = "\033[0m"; break;
    }

    printf("%s%s\033[0m", ansiColor, message);
#else
    printf("%s", message);
#endif
}

bool InitializeLogging() {
    if (!logFile.Open("console.log", FileMode::WRITE, false)) {
        LogOutput(LOG_LEVEL_ERROR, "Unable to open console.log for writing.");
        return false;
    }
    return true;
}

void ShutdownLogging() {
    logFile.Close();
}

void AppendToLogFile(const char* message) {
    if (logFile.IsOpen()) {
        const auto length = static_cast<uint64>(std::strlen(message));
        uint64 written = 0;
        if (!logFile.Write(length, message, &written)) {
            LogOutput(LOG_LEVEL_ERROR, "Error writing to console.log.");
        }
    }
}

void SetLogLevelEnabled(LogLevel level, bool enabled)
{
    if (level >= 0 && level < LOG_LEVEL_MAX)
        logLevelEnabled[level] = enabled;
}

bool IsLogLevelEnabled(LogLevel level)
{
    return (level >= 0 && level < LOG_LEVEL_MAX) && logLevelEnabled[level];
}

// -----------------------------------------------------------------------------
// 🧩 Compatibility overload (keeps older code working)
// -----------------------------------------------------------------------------
void LogOutput(LogLevel level, const char* message, ...)
{
    if (IsLoggingPaused()) return;
    if (!IsLogLevelEnabled(level)) return;

    constexpr int BUFFER_SIZE = 16384;
    char formatBuffer[BUFFER_SIZE];

    va_list arg_ptr;
            va_start(arg_ptr, message);
    vsnprintf(formatBuffer, BUFFER_SIZE, message, arg_ptr);
            va_end(arg_ptr);

    // Call main function with default channel
    LogOutput(level, LogChannel::DEFAULT, "%s", formatBuffer);
}

void LogOutput(LogLevel level, LogChannel channel, const char* message, ...) {
    if (IsLoggingPaused()) return;
    if (!IsLogLevelEnabled(level)) return;

    const char* levelStrings[] = {
            "[FATAL]: ", "[ERROR]: ", "[WARN]: ",
            "[INFO]: ", "[DEBUG]: ", "[TRACE]: "
    };
    const uint8_t levelColor[] = { 64, 4, 6, 2, 1, 8 };

    constexpr int BUFFER_SIZE = 16384;
    char formatBuffer[BUFFER_SIZE];
    char finalBuffer[BUFFER_SIZE];

    va_list arg_ptr;
            va_start(arg_ptr, message);
    vsnprintf(formatBuffer, BUFFER_SIZE, message, arg_ptr);
            va_end(arg_ptr);

    snprintf(finalBuffer, BUFFER_SIZE, "%s%s\n",
             levelStrings[level], formatBuffer);

    PrintToConsoleColor(finalBuffer, levelColor[level]);
    AppendToLogFile(finalBuffer);

    {
        std::scoped_lock lock(logMutex);
        if (logHistory.size() >= MAX_LOG_HISTORY)
            logHistory.pop_front();
        logHistory.emplace_back(level, channel, TimeManager::graphicsTimer.ReadSec(), std::string(finalBuffer));
    }

    if (logCallback)
        logCallback(level, channel, TimeManager::graphicsTimer.ReadSec(), finalBuffer);
}

void SetLoggingPaused(bool paused) { loggingPaused = paused; }
bool IsLoggingPaused() { return loggingPaused.load(); }

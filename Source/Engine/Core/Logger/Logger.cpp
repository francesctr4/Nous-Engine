#include "Logger.h"
#include "Asserts.h"
#include "Engine/Core/FileSystem/FileHandle/include/FileHandle.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <sstream>
#include <deque>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif

// ──────────────────────────────────────────────────────────────────────────────
// Configuration
// ──────────────────────────────────────────────────────────────────────────────

static constexpr size_t k_MaxLogHistory   = 10000;
static constexpr int    k_FormatBufSize   = 16384;

// ──────────────────────────────────────────────────────────────────────────────
// Log level / pause state
// ──────────────────────────────────────────────────────────────────────────────

static std::atomic<bool> s_loggingPaused{false};

// Plain bools — written only from UI thread, benign race on the read side.
static bool s_logLevelEnabled[LOG_LEVEL_MAX] = {
    true, true, true, true, true, false  // TRACE off by default
};

// ──────────────────────────────────────────────────────────────────────────────
// Log timer — independent of graphicsTimer so timestamps start from main()
// ──────────────────────────────────────────────────────────────────────────────

static std::chrono::steady_clock::time_point s_logEpoch; // zero-initialised (not yet set)
static bool                                  s_logEpochSet = false;

// ──────────────────────────────────────────────────────────────────────────────
// File
// ──────────────────────────────────────────────────────────────────────────────

static FileHandle s_logFile;
static std::mutex s_fileMutex; // serialises all writes to s_logFile

// ──────────────────────────────────────────────────────────────────────────────
// Ring buffer (committed history)
// ──────────────────────────────────────────────────────────────────────────────

static std::deque<LogEntry> s_ringBuffer;
static uint64_t             s_totalWritten = 0; // monotonic, never reset by ClearLogHistory
static std::mutex           s_historyMutex;

// ──────────────────────────────────────────────────────────────────────────────
// Callback
// ──────────────────────────────────────────────────────────────────────────────

static std::function<void(LogLevel, LogChannel, double, const char*)> s_logCallback;
static std::mutex s_callbackMutex;

// ──────────────────────────────────────────────────────────────────────────────
// Async flush queue
// ──────────────────────────────────────────────────────────────────────────────

static std::vector<LogEntry>       s_pendingQueue;
static std::mutex                  s_queueMutex;
static std::condition_variable     s_flushCV;
static std::thread                 s_flushThread;
static std::atomic<bool>           s_running{false};

// ──────────────────────────────────────────────────────────────────────────────
// Platform console color printing
// ──────────────────────────────────────────────────────────────────────────────

static void PrintToConsoleColor(const char* message, uint8_t color)
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD savedAttributes = consoleInfo.wAttributes;

    WORD windowsColor;
    switch (color) {
        case 64: windowsColor = FOREGROUND_RED | FOREGROUND_INTENSITY;           break;
        case 4:  windowsColor = FOREGROUND_RED;                                  break;
        case 6:  windowsColor = FOREGROUND_RED | FOREGROUND_GREEN;               break;
        case 2:  windowsColor = FOREGROUND_GREEN;                                break;
        case 1:  windowsColor = FOREGROUND_BLUE;                                 break;
        case 8:  windowsColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        default: windowsColor = savedAttributes;                                 break;
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
        default: ansiColor = "\033[0m";    break;
    }
    printf("%s%s\033[0m", ansiColor, message);
#else
    printf("%s", message);
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// Entry processing — called only from the flush thread (or synchronous fallback)
// ──────────────────────────────────────────────────────────────────────────────

static void ProcessEntry(const LogEntry& entry)
{
    // 1. Write to file (serialised with AppendToLogFile's direct writes)
    {
        std::lock_guard lock(s_fileMutex);
        if (s_logFile.IsOpen()) {
            const uint64 len = static_cast<uint64>(entry.message.size());
            uint64 written = 0;
            s_logFile.Write(len, entry.message.c_str(), &written);
        }
    }

    // 2. Commit to ring buffer
    {
        std::lock_guard lock(s_historyMutex);
        if (s_ringBuffer.size() >= k_MaxLogHistory)
            s_ringBuffer.pop_front();
        s_ringBuffer.push_back(entry);
        ++s_totalWritten;
    }

    // 3. Invoke callback — snapshot under lock, call outside to avoid deadlock
    std::function<void(LogLevel, LogChannel, double, const char*)> cb;
    {
        std::lock_guard lock(s_callbackMutex);
        cb = s_logCallback;
    }
    if (cb)
        cb(entry.level, entry.channel, entry.timestamp, entry.message.c_str());
}

// ──────────────────────────────────────────────────────────────────────────────
// Flush thread
// ──────────────────────────────────────────────────────────────────────────────

static void FlushThreadFunc()
{
    std::vector<LogEntry> batch;

    while (true)
    {
        {
            std::unique_lock lock(s_queueMutex);
            s_flushCV.wait(lock, [] {
                return !s_pendingQueue.empty() || !s_running.load(std::memory_order_relaxed);
            });

            // If shutdown was signalled and the queue is empty, we're done.
            if (s_pendingQueue.empty() && !s_running.load(std::memory_order_relaxed))
                break;

            // Swap in O(1) — releases the lock before processing
            batch.swap(s_pendingQueue);
        }

        for (const auto& entry : batch)
            ProcessEntry(entry);

        batch.clear();
    }

    // Drain any entries that arrived between the last swap and s_running = false.
    {
        std::lock_guard lock(s_queueMutex);
        batch.swap(s_pendingQueue);
    }
    for (const auto& entry : batch)
        ProcessEntry(entry);
}

// ──────────────────────────────────────────────────────────────────────────────
// Internal formatting helper shared by both public LogOutput overloads.
// Eliminates the double-format that existed in the old compat overload.
// ──────────────────────────────────────────────────────────────────────────────

static const char* k_LevelStrings[] = {
    "[FATAL]: ", "[ERROR]: ", "[WARN]: ",
    "[INFO]: ",  "[DEBUG]: ", "[TRACE]: "
};
// Short tags used in the verbose console line (no trailing colon/space).
static const char* k_LevelTags[] = {
    "[FATAL]", "[ERROR]", "[WARN]",
    "[INFO]",  "[DEBUG]", "[TRACE]"
};
static const uint8_t k_LevelColors[] = { 64, 4, 6, 2, 1, 8 };

static void LogOutputInternal(LogLevel level, LogChannel channel,
                              const char* file, int line, const char* function,
                              const char* userFormatted)
{
    // Stored message — "[LEVEL]: text\n" — ConsoleWindow prepends its own prefixes.
    char finalBuffer[k_FormatBufSize];
    snprintf(finalBuffer, k_FormatBufSize, "%s%s\n", k_LevelStrings[level], userFormatted);

    // Timestamp relative to s_logEpoch (set at the top of main via StartLogTimer).
    const double timestamp = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - s_logEpoch).count();

    // Thread ID — OS-level on Windows, hashed std::thread::id elsewhere.
#ifdef _WIN32
    const uint64_t threadId = static_cast<uint64_t>(GetCurrentThreadId());
#else
    const uint64_t threadId = static_cast<uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif

    // Verbose console line — mirrors the ConsoleWindow "Details" format.
    // [MM:SS:mmm] [LEVEL] [CHANNEL] filename:line [func] (tid:N) text
    {
        const int totalMs = static_cast<int>(timestamp * 1000.0);
        char consoleBuffer[k_FormatBufSize];

        if (file)
            snprintf(consoleBuffer, k_FormatBufSize,
                     "[%02d:%02d:%03d] %s [%s] %s:%d [%s] (tid:%llu) %s\n",
                     (totalMs / 1000) / 60, (totalMs / 1000) % 60, totalMs % 1000,
                     k_LevelTags[level],
                     LOG_CHANNEL_NAMES[(int)channel],
                     file, line,
                     function ? function : "",
                     static_cast<unsigned long long>(threadId),
                     userFormatted);
        else
            snprintf(consoleBuffer, k_FormatBufSize,
                     "[%02d:%02d:%03d] %s [%s] (tid:%llu) %s\n",
                     (totalMs / 1000) / 60, (totalMs / 1000) % 60, totalMs % 1000,
                     k_LevelTags[level],
                     LOG_CHANNEL_NAMES[(int)channel],
                     static_cast<unsigned long long>(threadId),
                     userFormatted);

        PrintToConsoleColor(consoleBuffer, k_LevelColors[level]);
    }

    // entry.message stays as "[LEVEL]: text\n" — ConsoleWindow adds its own timestamp/channel prefix.
    LogEntry entry;
    entry.level     = level;
    entry.channel   = channel;
    entry.timestamp = timestamp;
    entry.message   = std::string(finalBuffer);
    entry.file      = file;
    entry.line      = line;
    entry.function  = function;
    entry.threadId  = threadId;

    if (s_running.load(std::memory_order_relaxed))
    {
        // Async path — push to queue and wake the flush thread.
        {
            std::lock_guard lock(s_queueMutex);
            s_pendingQueue.push_back(std::move(entry));
        }
        s_flushCV.notify_one();
    }
    else
    {
        // Synchronous fallback — before InitializeLogging() or after ShutdownLogging().
        ProcessEntry(entry);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API — lifecycle
// ──────────────────────────────────────────────────────────────────────────────

void StartLogTimer()
{
    if (!s_logEpochSet) {
        s_logEpoch    = std::chrono::steady_clock::now();
        s_logEpochSet = true;
    }
}

bool InitializeLogging(bool enableFileLog)
{
    StartLogTimer(); // fallback: anchors to InitializeLogging() call if not called earlier

    if (enableFileLog)
    {
        if (!s_logFile.Open("console.log", FileMode::WRITE, false)) {
            // Can't use LogOutput yet — print directly to stderr.
            fprintf(stderr, "[Logger] Unable to open console.log for writing.\n");
            return false;
        }
    }

    s_running = true;
    s_flushThread = std::thread(FlushThreadFunc);
    return true;
}

void ShutdownLogging()
{
    // Signal the flush thread to stop once the queue is drained.
    {
        std::lock_guard lock(s_queueMutex);
        s_running = false;
    }
    s_flushCV.notify_one();

    if (s_flushThread.joinable())
        s_flushThread.join();

    s_logFile.Close();
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API — log output
// ──────────────────────────────────────────────────────────────────────────────

void LogOutput(LogLevel level, const char* message, ...)
{
    if (s_loggingPaused.load(std::memory_order_relaxed)) return;
    if (!IsLogLevelEnabled(level)) return;

    char buf[k_FormatBufSize];
    va_list args;
    va_start(args, message);
    vsnprintf(buf, k_FormatBufSize, message, args);
    va_end(args);

    LogOutputInternal(level, LogChannel::DEFAULT, nullptr, 0, nullptr, buf);
}

void LogOutput(LogLevel level, LogChannel channel, const char* message, ...)
{
    if (s_loggingPaused.load(std::memory_order_relaxed)) return;
    if (!IsLogLevelEnabled(level)) return;

    char buf[k_FormatBufSize];
    va_list args;
    va_start(args, message);
    vsnprintf(buf, k_FormatBufSize, message, args);
    va_end(args);

    LogOutputInternal(level, channel, nullptr, 0, nullptr, buf);
}

void LogOutputEx(LogLevel level, LogChannel channel,
                 const char* file, int line, const char* function,
                 const char* message, ...)
{
    if (s_loggingPaused.load(std::memory_order_relaxed)) return;
    if (!IsLogLevelEnabled(level)) return;

    char buf[k_FormatBufSize];
    va_list args;
    va_start(args, message);
    vsnprintf(buf, k_FormatBufSize, message, args);
    va_end(args);

    LogOutputInternal(level, channel, file, line, function, buf);
}

void LogOutputMultiline(LogLevel level, LogChannel channel, const char* text)
{
    if (s_loggingPaused.load(std::memory_order_relaxed)) return;
    if (!IsLogLevelEnabled(level)) return;

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty())
            LogOutputInternal(level, channel, nullptr, 0, nullptr, line.c_str());
    }
}

void LogOutputMultilineEx(LogLevel level, LogChannel channel,
                          const char* file, int line, const char* function,
                          const char* text)
{
    if (s_loggingPaused.load(std::memory_order_relaxed)) return;
    if (!IsLogLevelEnabled(level)) return;

    std::istringstream stream(text);
    std::string entry;
    while (std::getline(stream, entry)) {
        if (!entry.empty())
            LogOutputInternal(level, channel, file, line, function, entry.c_str());
    }
}

void AppendToLogFile(const char* message)
{
    std::lock_guard lock(s_fileMutex);
    if (s_logFile.IsOpen()) {
        const uint64 len = static_cast<uint64>(std::strlen(message));
        uint64 written = 0;
        if (!s_logFile.Write(len, message, &written))
            fprintf(stderr, "[Logger] AppendToLogFile: write failed.\n");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API — callback
// ──────────────────────────────────────────────────────────────────────────────

void SetLogCallback(std::function<void(LogLevel, LogChannel, double, const char*)> callback)
{
    std::lock_guard lock(s_callbackMutex);
    s_logCallback = std::move(callback);
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API — history (cursor-based)
// ──────────────────────────────────────────────────────────────────────────────

uint64_t GetLogEntryCount()
{
    std::lock_guard lock(s_historyMutex);
    return s_totalWritten;
}

uint64_t GetLogEntriesSince(uint64_t fromIndex, std::vector<LogEntry>& outEntries)
{
    std::lock_guard lock(s_historyMutex);

    if (!s_ringBuffer.empty())
    {
        const uint64_t oldestAvailable = s_totalWritten - static_cast<uint64_t>(s_ringBuffer.size());
        const uint64_t start           = (fromIndex > oldestAvailable) ? fromIndex : oldestAvailable;
        const size_t   offset          = static_cast<size_t>(start - oldestAvailable);

        outEntries.reserve(outEntries.size() + (s_ringBuffer.size() - offset));
        for (size_t i = offset; i < s_ringBuffer.size(); ++i)
            outEntries.push_back(s_ringBuffer[i]);
    }

    return s_totalWritten;
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API — history (legacy full-copy)
// ──────────────────────────────────────────────────────────────────────────────

std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> GetLogHistory()
{
    std::lock_guard lock(s_historyMutex);
    std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> result;
    for (const auto& e : s_ringBuffer)
        result.emplace_back(e.level, e.channel, e.timestamp, e.message);
    return result;
}

void ClearLogHistory()
{
    std::lock_guard lock(s_historyMutex);
    s_ringBuffer.clear();
    // s_totalWritten intentionally NOT reset — cursors held by ConsoleWindow
    // will simply return no entries until new ones are committed.
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API — runtime controls
// ──────────────────────────────────────────────────────────────────────────────

void SetLogLevelEnabled(LogLevel level, bool enabled)
{
    if (level >= 0 && level < LOG_LEVEL_MAX)
        s_logLevelEnabled[level] = enabled;
}

bool IsLogLevelEnabled(LogLevel level)
{
    return (level >= 0 && level < LOG_LEVEL_MAX) && s_logLevelEnabled[level];
}

void SetLoggingPaused(bool paused) { s_loggingPaused.store(paused); }
bool IsLoggingPaused()             { return s_loggingPaused.load(); }

// ──────────────────────────────────────────────────────────────────────────────
// Asserts
// ──────────────────────────────────────────────────────────────────────────────

void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line)
{
    LogOutput(LOG_LEVEL_FATAL,
              "Assertion Failure: %s, message: '%s', in file: %s, line: %d",
              expression, message, file, line);
}

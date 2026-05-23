#include <gtest/gtest.h>

#include "Engine/Core/Logger/Logger.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

static void WaitForLogEntry(uint64_t cursorBefore, int timeoutMs = 500)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (GetLogEntryCount() <= cursorBefore &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

// =============================================================================
// Fixture — initialize/shutdown the logger around each test
// =============================================================================

class t_Logger : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InitializeLogging(false); // no file output in tests
    }

    void TearDown() override
    {
        ShutdownLogging();
    }
};

// =============================================================================
// Log level controls
// =============================================================================

TEST_F(t_Logger, FatalLevel_EnabledByDefault)
{
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_FATAL));
}

TEST_F(t_Logger, ErrorLevel_EnabledByDefault)
{
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_ERROR));
}

TEST_F(t_Logger, WarnLevel_EnabledByDefault)
{
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_WARN));
}

TEST_F(t_Logger, InfoLevel_EnabledByDefault)
{
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_INFO));
}

TEST_F(t_Logger, SetLogLevelEnabled_False_DisablesLevel)
{
    SetLogLevelEnabled(LOG_LEVEL_INFO, false);
    EXPECT_FALSE(IsLogLevelEnabled(LOG_LEVEL_INFO));
}

TEST_F(t_Logger, SetLogLevelEnabled_TrueAfterFalse_ReEnables)
{
    SetLogLevelEnabled(LOG_LEVEL_WARN, false);
    EXPECT_FALSE(IsLogLevelEnabled(LOG_LEVEL_WARN));
    SetLogLevelEnabled(LOG_LEVEL_WARN, true);
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_WARN));
}

TEST_F(t_Logger, DisablingOneLevel_DoesNotAffectOthers)
{
    SetLogLevelEnabled(LOG_LEVEL_INFO, false);
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_ERROR));
    EXPECT_TRUE(IsLogLevelEnabled(LOG_LEVEL_WARN));
    SetLogLevelEnabled(LOG_LEVEL_INFO, true);
}

// =============================================================================
// Pause / resume
// =============================================================================

TEST_F(t_Logger, IsLoggingPaused_FalseByDefault)
{
    EXPECT_FALSE(IsLoggingPaused());
}

TEST_F(t_Logger, SetLoggingPaused_True_IsPaused)
{
    SetLoggingPaused(true);
    EXPECT_TRUE(IsLoggingPaused());
    SetLoggingPaused(false); // restore
}

TEST_F(t_Logger, SetLoggingPaused_False_IsNotPaused)
{
    SetLoggingPaused(true);
    SetLoggingPaused(false);
    EXPECT_FALSE(IsLoggingPaused());
}

// =============================================================================
// History — cursor-based pull API
// =============================================================================

TEST_F(t_Logger, GetLogEntryCount_IncreasesAfterLogging)
{
    const uint64_t before = GetLogEntryCount();
    LogOutput(LOG_LEVEL_INFO, "t_Logger test entry");
    WaitForLogEntry(before);
    EXPECT_GT(GetLogEntryCount(), before);
}

TEST_F(t_Logger, GetLogEntriesSince_ReturnsNewEntries)
{
    const uint64_t cursor0 = GetLogEntryCount();
    LogOutput(LOG_LEVEL_INFO, "t_Logger cursor test");
    WaitForLogEntry(cursor0);

    std::vector<LogEntry> entries;
    GetLogEntriesSince(cursor0, entries);
    EXPECT_FALSE(entries.empty());
}

TEST_F(t_Logger, GetLogEntriesSince_EntryHasCorrectLevel)
{
    const uint64_t cursor0 = GetLogEntryCount();
    LogOutput(LOG_LEVEL_WARN, "t_Logger level check");
    WaitForLogEntry(cursor0);

    std::vector<LogEntry> entries;
    GetLogEntriesSince(cursor0, entries);
    ASSERT_FALSE(entries.empty());
    EXPECT_EQ(entries.back().level, LOG_LEVEL_WARN);
}

// =============================================================================
// Callback
// =============================================================================

TEST_F(t_Logger, SetLogCallback_InvokedOnLog)
{
    std::atomic<bool> called = false;
    SetLogCallback([&called](LogLevel, LogChannel, double, const char*)
    {
        called = true;
    });

    const uint64_t cursor0 = GetLogEntryCount();
    LogOutput(LOG_LEVEL_INFO, "callback test");
    WaitForLogEntry(cursor0);

    EXPECT_TRUE(called);

    SetLogCallback(nullptr); // unregister to avoid dangling reference
}

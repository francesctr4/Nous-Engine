#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#include "Engine/Core/FileWatcher/FileWatcher.h"

namespace fs = std::filesystem;

class t_FileWatcher : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a temporary file to watch
        m_testFile = fs::temp_directory_path() / "nous_filewatcher_test.tmp";
        std::ofstream{ m_testFile } << "initial";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove(m_testFile, ec);
    }

    fs::path m_testFile;
    FileWatcher m_watcher;
};

TEST_F(t_FileWatcher, NofireOnRegistration)
{
    int callCount = 0;
    m_watcher.Watch(m_testFile.string(), [&](const std::string&) { ++callCount; });

    m_watcher.Poll();

    EXPECT_EQ(callCount, 0) << "Callback should not fire immediately after Watch()";
}

TEST_F(t_FileWatcher, FiresOnFileModification)
{
    int callCount = 0;
    std::string receivedPath;
    m_watcher.Watch(m_testFile.string(), [&](const std::string& p) {
        ++callCount;
        receivedPath = p;
    });

    // Ensure enough time passes that the filesystem timestamp changes
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ofstream{ m_testFile } << "modified";

    m_watcher.Poll();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(receivedPath, m_testFile.string());
}

TEST_F(t_FileWatcher, DoesNotFireTwiceWithoutChange)
{
    int callCount = 0;
    m_watcher.Watch(m_testFile.string(), [&](const std::string&) { ++callCount; });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ofstream{ m_testFile } << "modified";

    m_watcher.Poll(); // fires once
    m_watcher.Poll(); // should not fire again

    EXPECT_EQ(callCount, 1);
}

TEST_F(t_FileWatcher, UnwatchStopsCallbacks)
{
    int callCount = 0;
    m_watcher.Watch(m_testFile.string(), [&](const std::string&) { ++callCount; });

    m_watcher.Unwatch(m_testFile.string());

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ofstream{ m_testFile } << "modified";

    m_watcher.Poll();

    EXPECT_EQ(callCount, 0);
}

TEST_F(t_FileWatcher, ClearStopsAllCallbacks)
{
    int callCount = 0;
    m_watcher.Watch(m_testFile.string(), [&](const std::string&) { ++callCount; });

    m_watcher.Clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ofstream{ m_testFile } << "modified";

    m_watcher.Poll();

    EXPECT_EQ(callCount, 0);
}

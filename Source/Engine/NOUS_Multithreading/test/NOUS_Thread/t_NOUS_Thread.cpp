#include <gtest/gtest.h>

#include <NOUS_Multithreading/NOUS_Thread.h>
#include <NOUS_Multithreading/NOUS_Job.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace nous::engine::multithreading;

// =====================================================
// Construction / default state
// =====================================================

TEST(t_NOUS_Thread, DefaultStateIsReady)
{
    NOUS_Thread thread;
    EXPECT_EQ(thread.GetThreadState(), ThreadState::READY);
    EXPECT_FALSE(thread.IsRunning());
    EXPECT_EQ(thread.GetID(), std::thread::id{});
    EXPECT_EQ(thread.GetCurrentJob(), nullptr);
    EXPECT_EQ(thread.GetName(), "");
}

// =====================================================
// Start / Join / IsRunning
// =====================================================

TEST(t_NOUS_Thread, StartExecutesFunction)
{
    NOUS_Thread thread;
    std::atomic<bool> ran = false;

    thread.Start([&ran](){ ran = true; });
    thread.Join();

    EXPECT_TRUE(ran);
}

TEST(t_NOUS_Thread, IsRunningIsTrueWhileExecuting)
{
    NOUS_Thread thread;
    std::atomic<bool> started = false;
    std::atomic<bool> release = false;

    thread.Start([&]()
    {
        started = true;
        while (!release) std::this_thread::yield();
    });

    while (!started) std::this_thread::yield();
    EXPECT_TRUE(thread.IsRunning());

    release = true;
    thread.Join();
}

TEST(t_NOUS_Thread, IsRunningIsFalseAfterJoin)
{
    NOUS_Thread thread;
    thread.Start([]{});
    thread.Join();
    EXPECT_FALSE(thread.IsRunning());
}

// =====================================================
// Name
// =====================================================

TEST(t_NOUS_Thread, SetNameGetName)
{
    NOUS_Thread thread;
    thread.SetName("WorkerThread");
    EXPECT_EQ(thread.GetName(), "WorkerThread");
}

// =====================================================
// Thread state
// =====================================================

TEST(t_NOUS_Thread, SetGetThreadState)
{
    NOUS_Thread thread;
    thread.SetThreadState(ThreadState::RUNNING);
    EXPECT_EQ(thread.GetThreadState(), ThreadState::RUNNING);
    thread.SetThreadState(ThreadState::READY);
    EXPECT_EQ(thread.GetThreadState(), ThreadState::READY);
}

TEST(t_NOUS_Thread, GetStringFromStateReady)
{
    EXPECT_EQ(NOUS_Thread::GetStringFromState(ThreadState::READY), "READY");
}

TEST(t_NOUS_Thread, GetStringFromStateRunning)
{
    EXPECT_EQ(NOUS_Thread::GetStringFromState(ThreadState::RUNNING), "RUNNING");
}

// =====================================================
// Current job
// =====================================================

TEST(t_NOUS_Thread, SetGetCurrentJob)
{
    NOUS_Thread thread;
    NOUS_Job job("TestJob", [](){});

    EXPECT_EQ(thread.GetCurrentJob(), nullptr);
    thread.SetCurrentJob(&job);
    EXPECT_EQ(thread.GetCurrentJob(), &job);
    thread.SetCurrentJob(nullptr);
    EXPECT_EQ(thread.GetCurrentJob(), nullptr);
}

// =====================================================
// Execution timer
// =====================================================

TEST(t_NOUS_Thread, TimerReportsPositiveElapsedTimeAfterStop)
{
    NOUS_Thread thread;
    thread.StartExecutionTimer();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread.StopExecutionTimer();

    EXPECT_GT(thread.GetExecutionTimeMS(), 0.0);
}

TEST(t_NOUS_Thread, TimerWhileRunningReturnsNonNegativeElapsed)
{
    NOUS_Thread thread;
    thread.StartExecutionTimer();
    const double elapsed = thread.GetExecutionTimeMS();
    thread.StopExecutionTimer();

    EXPECT_GE(elapsed, 0.0);
}

// =====================================================
// Thread ID
// =====================================================

TEST(t_NOUS_Thread, GetDisplayIDIsConsistentForSameThread)
{
    const std::thread::id id = std::this_thread::get_id();
    EXPECT_EQ(NOUS_Thread::GetDisplayID(id), NOUS_Thread::GetDisplayID(id));
}

TEST(t_NOUS_Thread, GetDisplayIDDiffersForDifferentThreads)
{
    const std::thread::id mainId = std::this_thread::get_id();
    std::thread::id otherId;

    std::thread t([&otherId](){ otherId = std::this_thread::get_id(); });
    t.join();

    EXPECT_NE(mainId, otherId);
}

TEST(t_NOUS_Thread, SetThreadIDMatchesGetID)
{
    NOUS_Thread thread;
    const std::thread::id currentId = std::this_thread::get_id();
    thread.SetThreadID(currentId);
    EXPECT_EQ(thread.GetID(), currentId);
}

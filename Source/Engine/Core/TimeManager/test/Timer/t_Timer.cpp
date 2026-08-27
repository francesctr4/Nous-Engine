#include <gtest/gtest.h>

#include <TimeManager/Timer.h>

#include <thread>
#include <chrono>

// =============================================================================
// Initial state
// =============================================================================

TEST(t_Timer, DefaultStateIsStopped)
{
    Timer t;
    EXPECT_EQ(t.GetState(), TimerState::STOPPED);
}

TEST(t_Timer, ReadMS_WhenStopped_ReturnsZero)
{
    Timer t;
    EXPECT_FLOAT_EQ(t.ReadMS(), 0.f);
}

TEST(t_Timer, ReadSec_WhenStopped_ReturnsZero)
{
    Timer t;
    EXPECT_FLOAT_EQ(t.ReadSec(), 0.f);
}

// =============================================================================
// Start
// =============================================================================

TEST(t_Timer, Start_SetsStateRunning)
{
    Timer t;
    t.Start();
    EXPECT_EQ(t.GetState(), TimerState::RUNNING);
}

TEST(t_Timer, ReadMS_AfterSleep_IsPositive)
{
    Timer t;
    t.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_GT(t.ReadMS(), 0.f);
}

TEST(t_Timer, ReadSec_AfterSleep_IsPositive)
{
    Timer t;
    t.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_GT(t.ReadSec(), 0.f);
}

TEST(t_Timer, ReadMS_And_ReadSec_AreConsistent)
{
    Timer t;
    t.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    t.Stop(); // freeze before sampling so both reads see the same instant
    const float ms  = t.ReadMS();
    const float sec = t.ReadSec();
    EXPECT_NEAR(sec * 1000.f, ms, 1.f);
}

// =============================================================================
// Stop
// =============================================================================

TEST(t_Timer, Stop_SetsStateStopped)
{
    Timer t;
    t.Start();
    t.Stop();
    EXPECT_EQ(t.GetState(), TimerState::STOPPED);
}

TEST(t_Timer, Stop_FreezesReadMS)
{
    Timer t;
    t.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    t.Stop();
    const float atStop = t.ReadMS();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    EXPECT_NEAR(t.ReadMS(), atStop, 1.f);
}

// =============================================================================
// Pause / Resume
// =============================================================================

TEST(t_Timer, Pause_SetsStatePaused)
{
    Timer t;
    t.Start();
    t.Pause();
    EXPECT_EQ(t.GetState(), TimerState::PAUSED);
}

TEST(t_Timer, Pause_FreezesReadMS)
{
    Timer t;
    t.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    t.Pause();
    const float atPause = t.ReadMS();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_NEAR(t.ReadMS(), atPause, 1.f);
}

TEST(t_Timer, Resume_SetsStateRunning)
{
    Timer t;
    t.Start();
    t.Pause();
    t.Resume();
    EXPECT_EQ(t.GetState(), TimerState::RUNNING);
}

TEST(t_Timer, Resume_ContinuesAccumulatingTime)
{
    Timer t;
    t.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    t.Pause();
    const float beforeResume = t.ReadMS();
    t.Resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GT(t.ReadMS(), beforeResume);
}

// =============================================================================
// SetTimeScale
// =============================================================================

TEST(t_Timer, SetTimeScale_DoesNotCrash)
{
    Timer t;
    t.SetTimeScale(2.f);
    t.Start();
    EXPECT_EQ(t.GetState(), TimerState::RUNNING);
}

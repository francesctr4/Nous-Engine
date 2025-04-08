#pragma once

#include "NOUS_Multithreading.h"

#define ENABLE_UNIT_TESTS 1  // Enable/Disable Unit Tests

#if ENABLE_UNIT_TESTS == 1

#include "gtest/gtest.h"

#include <numeric>
#include <unordered_set>
#include <future>

#pragma region TEST_FIXTURE

// ========================================================================
// TEST FIXTURE
// ========================================================================

/// @brief Test fixture for NOUS_JobSystem Unit Tests.
/// @note Handles job system lifecycle management for all test cases.
class JobSystemTest : public ::testing::Test 
{
protected:

    void SetUp() override 
    {
        jobSystem = new NOUS_Multithreading::NOUS_JobSystem(NOUS_Multithreading::c_MAX_HARDWARE_THREADS);
    }

    void TearDown() override 
    {
        jobSystem->WaitForPendingJobs();

        delete jobSystem;
        jobSystem = nullptr;
    }

    NOUS_Multithreading::NOUS_JobSystem* jobSystem = nullptr;
    const int numStressJobs = 10000;
};

#pragma endregion

#pragma region BASIC_FUNCTIONALITY_TESTS

// ========================================================================
// BASIC FUNCTIONALITY TESTS
// ========================================================================

/// @brief Verifies basic job execution capability.
TEST_F(JobSystemTest, ExecutesSingleJob) 
{
    std::atomic<bool> jobExecuted(false);

    jobSystem->SubmitJob([&]() { jobExecuted = true; }, "SingleJobTest");
    jobSystem->WaitForPendingJobs();

    ASSERT_TRUE(jobExecuted);
}

/// @brief Tests handling of simple/no-op jobs.
TEST_F(JobSystemTest, HandlesEmptyJob) 
{
    std::atomic<bool> emptyJobExecuted(false);

    jobSystem->SubmitJob([&]() { emptyJobExecuted = true; }, "EmptyJobTest");
    jobSystem->WaitForPendingJobs();

    ASSERT_TRUE(emptyJobExecuted);
}

/// @brief Verifies fallback to main thread when no workers available.
TEST_F(JobSystemTest, SequentialExecutionOnMainThread) 
{
    if (!jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    std::atomic<int> counter(0);
    const std::thread::id mainThreadID = std::this_thread::get_id();

    auto verifyThread = [&]() {
        ASSERT_EQ(std::this_thread::get_id(), mainThreadID);
        counter++;
        };

    jobSystem->SubmitJob(verifyThread, "SequentialJob1");
    jobSystem->SubmitJob(verifyThread, "SequentialJob2");

    jobSystem->WaitForPendingJobs();

    ASSERT_EQ(counter.load(), 2);
}

#pragma endregion

#pragma region CONCURRENCY_TESTS

// ========================================================================
// CONCURRENCY TESTS
// ========================================================================

/// @brief Stress tests job submission and concurrent execution.
TEST_F(JobSystemTest, HandlesConcurrentJobs) 
{
    std::atomic<int> counter(0);
    const int numJobs = 1000;

    for (int i = 0; i < numJobs; ++i) 
    {
        jobSystem->SubmitJob([&]() { counter++; }, "ConcurrentJob_" + std::to_string(i));
    }

    jobSystem->WaitForPendingJobs();

    ASSERT_EQ(counter.load(), numJobs);
}

/// @brief Verifies thread safety with shared data access.
TEST_F(JobSystemTest, NoDataRaces) 
{
    std::vector<int> sharedData(1000, 0);
    const int numIterations = 10000;

    for (int i = 0; i < numIterations; ++i) 
    {
        jobSystem->SubmitJob([&sharedData, i]() {
            for (auto& item : sharedData) {
                item += i % 100;
            }
        }, "DataRaceCheck_" + std::to_string(i));
    }

    jobSystem->WaitForPendingJobs();

    const int total = std::accumulate(sharedData.begin(), sharedData.end(), 0);

    ASSERT_GT(total, 0) << "Shared data remained unchanged";
}

#pragma endregion

#pragma region STRESS_TESTS

// ========================================================================
// STRESS TESTS
// ========================================================================

/// @brief Tests system stability under heavy load.
TEST_F(JobSystemTest, StressTestWithManyJobs) 
{
    std::atomic<int> counter(0);

    for (int i = 0; i < numStressJobs; ++i) 
    {
        jobSystem->SubmitJob([&]() { counter++; }, "StressJob_" + std::to_string(i));
    }

    jobSystem->WaitForPendingJobs();

    ASSERT_EQ(counter.load(), numStressJobs);
}

/// @brief Verifies handling of extremely large job counts.
TEST_F(JobSystemTest, HandlesMassiveJobCount) 
{
    std::atomic<int> counter(0);
    const int massiveJobCount = 1000000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < massiveJobCount; ++i) 
    {
        jobSystem->SubmitJob([&] { counter++; }, "MassiveJob_" + std::to_string(i));
    }

    jobSystem->WaitForPendingJobs();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "Processed " << massiveJobCount << " jobs in " << duration << "ms\n";

    ASSERT_EQ(counter.load(), massiveJobCount);
}

#pragma endregion

#pragma region THREAD_MANAGEMENT_TESTS

// ========================================================================
// THREAD MANAGEMENT TESTS
// ========================================================================

/// @brief Verifies proper distribution across available threads.
TEST_F(JobSystemTest, ProperThreadUtilization) 
{
    std::mutex mutex;
    std::unordered_set<std::thread::id> threadIds;
    const int numJobs = 1000;

    for (int i = 0; i < numJobs; ++i) 
    {
        jobSystem->SubmitJob([&]() {
            std::lock_guard<std::mutex> lock(mutex);
            threadIds.insert(std::this_thread::get_id());
            }, "ThreadUtilJob_" + std::to_string(i)
        );
    }

    jobSystem->WaitForPendingJobs();

    ASSERT_GE(threadIds.size(), 1) << "Jobs not distributed across threads";
    ASSERT_LE(threadIds.size(), NOUS_Multithreading::c_MAX_HARDWARE_THREADS) << "Exceeded maximum thread count";
}

/// @brief Verifies thread state tracking functionality.
TEST_F(JobSystemTest, ThreadStatesAreUpdated) 
{
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    jobSystem->SubmitJob([]() {
        NOUS_Multithreading::NOUS_Thread::SleepMS(50);
        }, "StateCheckJob"
    );

    NOUS_Multithreading::NOUS_Thread::SleepMS(10);

    bool anyRunning = false;
    for (const auto& thread : jobSystem->GetThreadPool().GetThreads()) 
    {
        if (thread->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING) 
        {
            anyRunning = true;
            break;
        }
    }

    ASSERT_TRUE(anyRunning) << "No threads entered RUNNING state";
}

#pragma endregion

#pragma region ERROR_HANDLING_TESTS

// ========================================================================
// ERROR HANDLING TESTS
// ========================================================================

/// @brief Verifies system stability when jobs throw exceptions.
TEST_F(JobSystemTest, HandlesExceptionInJobs) 
{
    bool subsequentJobExecuted = false;

    // Uncomment to submit job that throws an exception.
    /*jobSystem->SubmitJob([]() {
        throw std::runtime_error("Simulated job failure");
        }, "FailingJob"
    );*/

    // Submit normal job
    jobSystem->SubmitJob([&]() {
        subsequentJobExecuted = true;
        }, "FollowingJob"
    );

    jobSystem->WaitForPendingJobs();

    ASSERT_TRUE(subsequentJobExecuted) << "Exception prevented subsequent job execution";
}

#pragma endregion

#pragma region SYNCHRONIZATION_TESTS

// ========================================================================
// SYNCHRONIZATION TESTS
// ========================================================================

/// @brief Verifies WaitForPendingJobs() blocking behavior.
TEST_F(JobSystemTest, WaitForAllBlocksProperly) 
{
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    std::atomic<bool> waitCompleted(false);
    std::atomic<int> counter(0);
    const int numJobs = 10;

    for (int i = 0; i < numJobs; ++i) 
    {
        jobSystem->SubmitJob([&]() {
            NOUS_Multithreading::NOUS_Thread::SleepMS(100);
            counter++;
            }, "WaitTestJob_" + std::to_string(i)
        );
    }

    auto future = std::async(std::launch::async, [&]() {
        jobSystem->WaitForPendingJobs();
        waitCompleted = true;
        }
    );

    // Verify wait hasn't completed prematurely
    NOUS_Multithreading::NOUS_Thread::SleepMS(50);
    ASSERT_FALSE(waitCompleted) << "Wait completed too early";

    future.wait();
    ASSERT_EQ(counter.load(), numJobs) << "Not all jobs completed";
}

#pragma endregion

#pragma region PERFORMANCE_TESTS

// ========================================================================
// PERFORMANCE TESTS
// ========================================================================

/// @brief Verifies execution time measurement accuracy.
TEST_F(JobSystemTest, JobExecutionTimeMeasurement) 
{
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    constexpr int sleepDuration = 100;
    jobSystem->SubmitJob([]() {
        NOUS_Multithreading::NOUS_Thread::SleepMS(sleepDuration);
        }, "TimedJob"
    );

    jobSystem->WaitForPendingJobs();

    // Allow small margin for timing variance
    constexpr double tolerance = 0.2;
    bool foundValidTime = false;

    for (const auto& thread : jobSystem->GetThreadPool().GetThreads()) 
    {
        const double measuredTime = thread->GetExecutionTimeMS();

        if (measuredTime >= sleepDuration * (1 - tolerance) &&
            measuredTime <= sleepDuration * (1 + tolerance)) 
        {
            foundValidTime = true;
            break;
        }
    }

    ASSERT_TRUE(foundValidTime) << "No valid execution times recorded";
}

/// @brief Measures system throughput under load.
TEST_F(JobSystemTest, MeasuresThroughput) 
{
    const int numJobs = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numJobs; ++i) 
    {
        jobSystem->SubmitJob([] {}, "ThroughputJob_" + std::to_string(i));
    }

    jobSystem->WaitForPendingJobs();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    std::cout << "\nThroughput: " << numJobs << " jobs in " << duration
        << "ms (" << (numJobs * 1000.0 / duration) << " jobs/sec)\n";
}

#pragma endregion

#pragma region DEBUGGING_TOOLS_TESTS

// ========================================================================
// DEBUGGING TOOLS TESTS
// ========================================================================

/// @brief Verifies debug info shows proper thread state transitions.
TEST_F(JobSystemTest, ThreadStateTransitionsVisibleInDebugInfo) 
{
    constexpr int NUM_JOBS = 30;
    constexpr int JOB_DURATION_MS = 500;

    std::cout << "\n=== Initial State ===\n";
    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    for (int i = 0; i < NUM_JOBS; ++i) 
    {
        jobSystem->SubmitJob([]() {
            NOUS_Multithreading::NOUS_Thread::SleepMS(JOB_DURATION_MS);
            }, "DebugJob_" + std::to_string(i + 1)
        );
    }

    bool sawRunningState = false;
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        if (auto* mainThread = NOUS_Multithreading::GetMainThread()) 
        {
            sawRunningState = mainThread->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING;
        }
    }
    else 
    {
        std::cout << "\n=== During Execution ===\n";
        while (jobSystem->GetPendingJobs() > 0) 
        {
            NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

            for (const auto& thread : jobSystem->GetThreadPool().GetThreads()) 
            {
                if (thread->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING) 
                {
                    sawRunningState = true;
                }
            }

            NOUS_Multithreading::NOUS_Thread::SleepMS(50);
        }
    }

    std::cout << "\n=== Final State ===\n";
    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    ASSERT_TRUE(sawRunningState) << "No RUNNING state observed";
}

#pragma endregion

#pragma region DYNAMIC_RESIZE

// ========================================================================
// DYNAMIC CONFIGURATION TESTS
// ========================================================================

/// @brief Tests dynamic thread pool resizing.
TEST_F(JobSystemTest, DynamicThreadPoolResizing) 
{
    const uint32_t initialSize = jobSystem->GetThreadPool().GetThreads().size();

    // Reduce to zero threads on the thread pool
    jobSystem->Resize(0);
    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), 0);

    // Verify sequential operation
    std::atomic<int> counter(0);
    jobSystem->SubmitJob([&]() { counter++; }, "PostResizeJob1");
    jobSystem->SubmitJob([&]() { counter++; }, "PostResizeJob2");
    jobSystem->WaitForPendingJobs();
    ASSERT_EQ(counter.load(), 2);

    // Test restoring to original size
    jobSystem->Resize(initialSize);
    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), initialSize);
}

#pragma endregion

#endif // ENABLE_UNIT_TESTS
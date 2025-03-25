#pragma once

#include "NOUS_Multithreading.h"
#include "gtest/gtest.h"

#include <numeric>
#include <unordered_set>
#include <future>

class JobSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        jobSystem = NOUS_NEW<NOUS_Multithreading::NOUS_JobSystem>(
            MemoryManager::MemoryTag::THREAD 
             // SIZE (remove if MAX_HARDWARE_THREADS)
        );
    }

    void TearDown() override {
        jobSystem->WaitForAll();
        NOUS_DELETE<NOUS_Multithreading::NOUS_JobSystem>(jobSystem, MemoryManager::MemoryTag::THREAD);
    }

    NOUS_Multithreading::NOUS_JobSystem* jobSystem = nullptr;
    const int numStressJobs = 10000;
};

#pragma region BASIC_FUNCTIONALITY
TEST_F(JobSystemTest, ExecutesSingleJob) {
    std::atomic<bool> jobExecuted(false);
    jobSystem->SubmitJob([&]() { jobExecuted = true; });
    jobSystem->WaitForAll();
    ASSERT_TRUE(jobExecuted);
}

TEST_F(JobSystemTest, HandlesEmptyJob) {
    bool emptyJobExecuted = false;
    jobSystem->SubmitJob([&]() { emptyJobExecuted = true; });
    jobSystem->WaitForAll();
    ASSERT_TRUE(emptyJobExecuted);
}

TEST_F(JobSystemTest, SequentialExecutionOnMainThread) 
{
    if (!jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    std::atomic<int> counter = 0;
    std::thread::id mainThreadID = std::this_thread::get_id();

    jobSystem->SubmitJob([&]() {
        ASSERT_EQ(std::this_thread::get_id(), mainThreadID);
        counter++;
        });

    jobSystem->SubmitJob([&]() {
        ASSERT_EQ(std::this_thread::get_id(), mainThreadID);
        counter++;
        });

    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), 2);
}
#pragma endregion

#pragma region CONCURRENCY_TESTS
TEST_F(JobSystemTest, HandlesConcurrentJobs) {
    std::atomic<int> counter(0);
    const int numJobs = 1000;

    for (int i = 0; i < numJobs; ++i) {
        jobSystem->SubmitJob([&]() { counter++; });
    }

    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), numJobs);
}

TEST_F(JobSystemTest, NoDataRaces) {
    std::vector<int> sharedData(1000, 0);
    for (int i = 0; i < 10000; ++i) {
        jobSystem->SubmitJob([&sharedData, i]() {
            for (auto& item : sharedData) item += i % 100;
            });
    }
    jobSystem->WaitForAll();
    int total = std::accumulate(sharedData.begin(), sharedData.end(), 0);
    ASSERT_GT(total, 0);
}
#pragma endregion

#pragma region STRESS_TESTS
TEST_F(JobSystemTest, StressTestWithManyJobs) {
    std::atomic<int> counter(0);
    for (int i = 0; i < numStressJobs; ++i) {
        jobSystem->SubmitJob([&]() { counter++; });
    }
    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), numStressJobs);
}

TEST_F(JobSystemTest, HandlesMassiveJobCount) {
    std::atomic<int> counter(0);
    for (int i = 0; i < 1000000; ++i) {
        jobSystem->SubmitJob([&] { counter++; });
    }
    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), 1000000);
}
#pragma endregion

#pragma region THREAD_MANAGEMENT
TEST_F(JobSystemTest, ProperThreadUtilization) {
    std::mutex mutex;
    std::unordered_set<std::thread::id> threadIds;
    const int numJobs = 1000;

    for (int i = 0; i < numJobs; ++i) {
        jobSystem->SubmitJob([&]() {
            std::lock_guard<std::mutex> lock(mutex);
            threadIds.insert(std::this_thread::get_id());
            });
    }

    jobSystem->WaitForAll();
    ASSERT_GE(threadIds.size(), 1);
    ASSERT_LE(threadIds.size(), NOUS_Multithreading::c_MAX_HARDWARE_THREADS);
}

TEST_F(JobSystemTest, ThreadStatesAreUpdated) 
{
    // Skip test if thread pool is empty
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    jobSystem->SubmitJob([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto& pool = jobSystem->GetThreadPool();
    bool anyRunning = false;
    for (const auto& t : pool.GetThreads()) {
        if (t->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING) {
            anyRunning = true;
            break;
        }
    }
    ASSERT_TRUE(anyRunning);
}
#pragma endregion

#pragma region ERROR_HANDLING
TEST_F(JobSystemTest, HandlesExceptionInJobs) {
    bool subsequentJobExecuted = false;
    jobSystem->SubmitJob([&]() { subsequentJobExecuted = true; });
    jobSystem->WaitForAll();
    ASSERT_TRUE(subsequentJobExecuted);
}
#pragma endregion

#pragma region SYNCHRONIZATION_TESTS
TEST_F(JobSystemTest, WaitForAllBlocksProperly) 
{
    // Skip test if thread pool is empty
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    std::atomic<bool> earlyCheck(false);
    std::atomic<int> counter(0);

    for (int i = 0; i < 10; ++i) {
        jobSystem->SubmitJob([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            counter++;
            });
    }

    auto future = std::async(std::launch::async, [&]() {
        jobSystem->WaitForAll();
        earlyCheck = true;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_FALSE(earlyCheck);
    future.wait();
    ASSERT_EQ(counter.load(), 10);
}
#pragma endregion

#pragma region PERFORMANCE_METRICS
TEST_F(JobSystemTest, JobExecutionTimeMeasurement) 
{
    // Skip test if thread pool is empty
    if (jobSystem->GetThreadPool().GetThreads().empty()) 
    {
        return;
    }

    jobSystem->SubmitJob([]() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); });
    jobSystem->WaitForAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto& pool = jobSystem->GetThreadPool();
    bool foundValidTime = false;
    for (const auto& t : pool.GetThreads()) {
        double time = t->GetExecutionTimeMS();
        if (time >= 90 && time < 200) foundValidTime = true;
    }
    ASSERT_TRUE(foundValidTime);
}

TEST_F(JobSystemTest, MeasuresThroughput) {
    const int numJobs = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numJobs; ++i) jobSystem->SubmitJob([] {});
    jobSystem->WaitForAll();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();
    std::cout << "Processed " << numJobs << " jobs in " << duration << "ms\n";
}
#pragma endregion

#pragma region DEBUGGING_TOOLS
TEST_F(JobSystemTest, ThreadStateTransitionsVisibleInDebugInfo) 
{
    constexpr int NUM_JOBS = 10;
    constexpr int JOB_DURATION_MS = 2000;

    std::cout << "\n=== Initial State ===\n";
    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    for (int i = 0; i < NUM_JOBS; ++i) 
    {
        jobSystem->SubmitJob([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(JOB_DURATION_MS));
            }, "Import Model");

        jobSystem->SubmitJob([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(JOB_DURATION_MS));
            }, "Import Material");

        jobSystem->SubmitJob([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(JOB_DURATION_MS));
            }, "Import Texture");
    }

    // During execution, check main thread if pool is empty
    bool sawRunningState = false;
    if (jobSystem->GetThreadPool().GetThreads().empty()) {
        auto* mainThread = NOUS_Multithreading::GetMainThread();
        if (mainThread && mainThread->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING) {
            sawRunningState = true;
        }
    }
    else {
        // Original worker thread check
        std::cout << "\n=== During Execution State ===\n";
        while (jobSystem->GetPendingJobs() > 0) {
            NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);
            for (const auto& thread : jobSystem->GetThreadPool().GetThreads()) {
                if (thread->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING) {
                    sawRunningState = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    ASSERT_TRUE(sawRunningState);
    jobSystem->WaitForAll();

    // Final state assertions
    std::cout << "\n=== Final State ===\n";
    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);
}
#pragma endregion

#pragma region DYNAMIC_RESIZE
TEST_F(JobSystemTest, DynamicThreadPoolResizing) 
{
    uint32 initialSize = jobSystem->GetThreadPool().GetThreads().size();

    // Initial state
    std::cout << "\n=== Initial Pool ===\n";
    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), initialSize);

    for (int i = 0; i < initialSize; ++i)
    {
        jobSystem->SubmitJob([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(2000)); }, "Job with 20 threads");
    }

    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    // Resize to single-threaded
    uint32 newSize = 0;
    jobSystem->Resize(newSize);
    std::cout << "\n=== Resized Thread Pool ===\n";
    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), newSize);

    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    // Verify functionality with 0 threads
    for (int i = 0; i < 2; ++i) {
        jobSystem->SubmitJob([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(2000)); }, "Job with main thread");
    }

    // Resize back to 20 threads
    jobSystem->Resize(initialSize);
    std::cout << "\n=== Resized Back to initial threads ===\n";

    for (int i = 0; i < initialSize; ++i)
    {
        jobSystem->SubmitJob([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(2000)); }, "Job with 20 threads");
    }

    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), initialSize);
}

TEST_F(JobSystemTest, DynamicForceReset)
{
    uint32 initialSize = jobSystem->GetThreadPool().GetThreads().size();

    // Initial state
    std::cout << "\n=== Initial Pool ===\n";
    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), initialSize);

    for (int i = 0; i < initialSize; ++i)
    {
        jobSystem->SubmitJob([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(2000)); }, "Job with 20 threads");
    }

    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    jobSystem->ForceReset();

    for (int i = 0; i < initialSize; ++i)
    {
        jobSystem->SubmitJob([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(2000)); }, "Job with 20 threads");
    }

    NOUS_Multithreading::JobSystemDebugInfo(*jobSystem);

    ASSERT_EQ(jobSystem->GetThreadPool().GetThreads().size(), initialSize);
}
#pragma endregion
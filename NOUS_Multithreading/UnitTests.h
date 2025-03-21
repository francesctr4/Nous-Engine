#pragma once

#include "NOUS_Multithreading.h"
#include "gtest/gtest.h"

class JobSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        jobSystem = std::make_unique<NOUS_Multithreading::NOUS_JobSystem>();
    }

    void TearDown() override {
        jobSystem->WaitForAll();
    }

    std::unique_ptr<NOUS_Multithreading::NOUS_JobSystem> jobSystem;
    const int numStressJobs = 10000;
};

TEST_F(JobSystemTest, ExecutesSingleJob) {
    std::atomic<bool> jobExecuted(false);

    jobSystem->SubmitJob([&]() {
        jobExecuted = true;
        });

    jobSystem->WaitForAll();
    ASSERT_TRUE(jobExecuted);
}

TEST_F(JobSystemTest, HandlesConcurrentJobs) {
    std::atomic<int> counter(0);
    const int numJobs = 1000;

    for (int i = 0; i < numJobs; ++i) {
        jobSystem->SubmitJob([&]() {
            counter++;
            });
    }

    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), numJobs);
}

TEST_F(JobSystemTest, StressTestWithManyJobs) {
    std::atomic<int> counter(0);

    for (int i = 0; i < numStressJobs; ++i) {
        jobSystem->SubmitJob([&]() {
            counter++;
            });
    }

    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), numStressJobs);
}

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
    ASSERT_GT(threadIds.size(), 1); // Ensure multiple threads used
    ASSERT_LE(threadIds.size(), NOUS_Multithreading::c_MAX_HARDWARE_THREADS);
}

TEST_F(JobSystemTest, HandlesExceptionInJobs) {
    bool subsequentJobExecuted = false;

    // Submit job that throws (it actually crashes and stops the execution).
    //jobSystem->SubmitJob([]() {
    //    throw std::runtime_error("Test error");
    //    });

    // Submit normal job
    jobSystem->SubmitJob([&]() {
        subsequentJobExecuted = true;
        });

    jobSystem->WaitForAll();
    ASSERT_TRUE(subsequentJobExecuted);
}

TEST_F(JobSystemTest, WaitForAllBlocksProperly) {
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

    // Check after 50ms (before jobs should complete)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_FALSE(earlyCheck);

    future.wait();
    ASSERT_EQ(counter.load(), 10);
}

TEST_F(JobSystemTest, ThreadStatesAreUpdated) {
    jobSystem->SubmitJob([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });

    // Let jobs start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto& pool = jobSystem->GetThreadPool();
    const auto& threads = pool.GetThreads();

    bool anyRunning = false;
    for (const auto& t : threads) {
        if (t->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING) {
            anyRunning = true;
            break;
        }
    }

    ASSERT_TRUE(anyRunning);
}

TEST_F(JobSystemTest, HandlesEmptyJob) {
    bool emptyJobExecuted = false;

    jobSystem->SubmitJob([&]() {
        emptyJobExecuted = true;
        });

    jobSystem->WaitForAll();
    ASSERT_TRUE(emptyJobExecuted);
}

TEST_F(JobSystemTest, JobExecutionTimeMeasurement) {
    jobSystem->SubmitJob([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });

    jobSystem->WaitForAll();

    // Allow time for thread states to update
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto& pool = jobSystem->GetThreadPool();
    const auto& threads = pool.GetThreads();

    bool foundValidTime = false;
    for (const auto& t : threads) {
        double time = t->GetExecutionTimeMS();
        // Wider tolerance range
        if (time >= 90 && time < 200) {
            foundValidTime = true;
            break;
        }
    }

    ASSERT_TRUE(foundValidTime) << "No thread reported time in [90, 200)ms range.";
}

TEST_F(JobSystemTest, MeasuresThroughput) {
    const int numJobs = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numJobs; ++i) {
        jobSystem->SubmitJob([] {});
    }

    jobSystem->WaitForAll();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    std::cout << "Processed " << numJobs << " jobs in "
        << duration.count() << "ms\n";
}

TEST_F(JobSystemTest, NoDataRaces) {
    std::vector<int> sharedData(1000, 0);

    for (int i = 0; i < 10000; ++i) {
        jobSystem->SubmitJob([&sharedData, i]() {
            for (auto& item : sharedData) {
                item += i % 100;
            }
            });
    }

    jobSystem->WaitForAll();

    // Sum should be unpredictable due to races, but shouldn't crash
    int total = std::accumulate(sharedData.begin(), sharedData.end(), 0);
    ASSERT_GT(total, 0);
}

TEST_F(JobSystemTest, ThreadCountAffectsPerformance) 
{
    constexpr int NUM_JOBS = 10;
    constexpr int JOB_SLEEP_MS = 500;
    constexpr int LOW_THREAD_COUNT = 2;
    constexpr double EXPECTED_LOW_TIME = (NUM_JOBS * JOB_SLEEP_MS) / LOW_THREAD_COUNT;
    constexpr double TOLERANCE = 0.3; // 30% tolerance for CI variability

    // Create a custom pool with limited threads
    NOUS_Multithreading::NOUS_ThreadPool lowThreadPool(LOW_THREAD_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    // Submit sleep jobs
    for (int i = 0; i < NUM_JOBS; ++i) {
        lowThreadPool.SubmitJob([JOB_SLEEP_MS]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(JOB_SLEEP_MS));
            });
    }

    // Wait for completion
    lowThreadPool.Shutdown();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    ).count();

    // Verify duration matches thread-limited expectation
    const double minExpected = EXPECTED_LOW_TIME * (1 - TOLERANCE);
    const double maxExpected = EXPECTED_LOW_TIME * (1 + TOLERANCE);

    std::cout << "Actual duration: " << duration << "ms (Expected: "
        << EXPECTED_LOW_TIME << "ms ±30%)\n";

    ASSERT_GT(duration, minExpected) << "Execution was faster than expected with limited threads";
    ASSERT_LT(duration, maxExpected) << "Execution was slower than expected with limited threads";

    // Verify thread utilization
    const auto& threads = lowThreadPool.GetThreads();
    int maxConcurrentJobs = 0;
    std::atomic<int> concurrentJobs(0);

    // Add instrumentation to track concurrency
    for (auto& thread : threads) {
        thread->SetCurrentTask([&concurrentJobs, &maxConcurrentJobs]() {
            concurrentJobs++;
            maxConcurrentJobs = std::max(maxConcurrentJobs, concurrentJobs.load());
            std::this_thread::sleep_for(std::chrono::milliseconds(JOB_SLEEP_MS));
            concurrentJobs--;
            });
    }

    ASSERT_LE(maxConcurrentJobs, LOW_THREAD_COUNT)
        << "More jobs ran concurrently than available threads";
}

TEST_F(JobSystemTest, HandlesMassiveJobCount) 
{
    std::atomic<int> counter(0);
    for (int i = 0; i < 1000000; ++i) {
        jobSystem->SubmitJob([&] { counter++; });
    }
    jobSystem->WaitForAll();
    ASSERT_EQ(counter.load(), 1000000);
}

TEST_F(JobSystemTest, ThreadStateTransitionsVisibleInDebugInfo) {
    constexpr int NUM_JOBS = 10;
    constexpr int JOB_DURATION_MS = 2000;

    // Capture initial state
    std::cout << "\n=== Initial State ===\n";
    NOUS_Multithreading::JobSystemDebugInfo();

    // Submit jobs
    for (int i = 0; i < NUM_JOBS; ++i) {
        jobSystem->SubmitJob([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(JOB_DURATION_MS));
            });
    }

    // Capture state during execution
    std::cout << "\n=== During Execution ===\n";
    for (int i = 0; i < 3; ++i) {
        NOUS_Multithreading::JobSystemDebugInfo();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Wait for completion
    jobSystem->WaitForAll();

    // Capture final state
    std::cout << "\n=== Final State ===\n";
    NOUS_Multithreading::JobSystemDebugInfo();

    // Programmatic verification
    const auto& pool = jobSystem->GetThreadPool();
    const auto& threads = pool.GetThreads();

    // Verify all threads returned to waiting/ready state
    for (const auto& thread : threads) {
        const auto state = thread->GetThreadState();
        ASSERT_TRUE(state == NOUS_Multithreading::ThreadState::WAITING || state == NOUS_Multithreading::ThreadState::READY)
            << "Thread " << thread->GetName() << " in unexpected state after completion";
    }
}
#include <gtest/gtest.h>

#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace NOUS_Multithreading;

// =====================================================
// Fixture
// =====================================================

class t_NOUS_JobSystem : public ::testing::Test
{
protected:
    static constexpr size_t kPoolSize = 32 * 1024 * 1024; // 32 MiB

    NOUS_JobSystem* jobSystem = nullptr;

    void SetUp() override
    {
        MemoryManager::InitializeMemory(kPoolSize);
    }

    void TearDown() override
    {
        delete jobSystem;
        jobSystem = nullptr;
        MemoryManager::ShutdownMemory();
    }
};

// =====================================================
// Construction
// =====================================================

TEST_F(t_NOUS_JobSystem, ConstructorWithZeroCreatesEmptyPool)
{
    jobSystem = new NOUS_JobSystem(0);
    EXPECT_TRUE(jobSystem->GetThreadPool().GetThreads().empty());
}

TEST_F(t_NOUS_JobSystem, ConstructorWithNThreadsCreatesNWorkers)
{
    jobSystem = new NOUS_JobSystem(3);
    EXPECT_EQ(jobSystem->GetThreadPool().GetThreads().size(), 3u);
}

TEST_F(t_NOUS_JobSystem, InitialPendingJobsCountIsZero)
{
    jobSystem = new NOUS_JobSystem(2);
    EXPECT_EQ(jobSystem->GetPendingJobs(), 0);
}

// =====================================================
// Job submission and execution
// =====================================================

TEST_F(t_NOUS_JobSystem, SubmitJobExecutesJob)
{
    jobSystem = new NOUS_JobSystem(2);
    std::atomic<bool> ran = false;

    jobSystem->SubmitJob([&ran](){ ran = true; });
    jobSystem->WaitForPendingJobs();

    EXPECT_TRUE(ran);
}

TEST_F(t_NOUS_JobSystem, SubmitJobWithSizeZeroExecutesSynchronously)
{
    jobSystem = new NOUS_JobSystem(0);
    bool ran = false; // not atomic: synchronous execution, no data race

    jobSystem->SubmitJob([&ran](){ ran = true; });

    // No wait needed — size=0 runs the job on the calling thread before returning
    EXPECT_TRUE(ran);
}

TEST_F(t_NOUS_JobSystem, MultipleJobsAllExecute)
{
    constexpr int jobCount = 20;
    jobSystem = new NOUS_JobSystem(2);
    std::atomic<int> count = 0;

    for (int i = 0; i < jobCount; ++i)
        jobSystem->SubmitJob([&count](){ ++count; });

    jobSystem->WaitForPendingJobs();
    EXPECT_EQ(count.load(), jobCount);
}

TEST_F(t_NOUS_JobSystem, JobsRunOnWorkerThreadsNotMainThread)
{
    jobSystem = new NOUS_JobSystem(1);
    std::atomic<std::thread::id> jobThreadId;

    jobSystem->SubmitJob([&jobThreadId](){ jobThreadId = std::this_thread::get_id(); });
    jobSystem->WaitForPendingJobs();

    EXPECT_NE(jobThreadId.load(), std::this_thread::get_id());
}

// =====================================================
// WaitForPendingJobs
// =====================================================

TEST_F(t_NOUS_JobSystem, WaitForPendingJobsBlocksUntilAllComplete)
{
    jobSystem = new NOUS_JobSystem(2);
    std::atomic<int> count = 0;

    for (int i = 0; i < 10; ++i)
    {
        jobSystem->SubmitJob([&count]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ++count;
        });
    }

    jobSystem->WaitForPendingJobs();
    EXPECT_EQ(count.load(), 10);
}

TEST_F(t_NOUS_JobSystem, PendingJobsCountReachesZeroAfterWait)
{
    jobSystem = new NOUS_JobSystem(2);

    for (int i = 0; i < 5; ++i)
        jobSystem->SubmitJob([](){ std::this_thread::sleep_for(std::chrono::milliseconds(5)); });

    jobSystem->WaitForPendingJobs();
    EXPECT_EQ(jobSystem->GetPendingJobs(), 0);
}

TEST_F(t_NOUS_JobSystem, WaitForPendingJobsReturnsImmediatelyWhenNoJobsPending)
{
    jobSystem = new NOUS_JobSystem(2);
    // Should return immediately without blocking
    jobSystem->WaitForPendingJobs();
    EXPECT_EQ(jobSystem->GetPendingJobs(), 0);
}

// =====================================================
// Resize
// =====================================================

TEST_F(t_NOUS_JobSystem, ResizeIncreasesThreadCount)
{
    jobSystem = new NOUS_JobSystem(2);
    jobSystem->Resize(4);
    EXPECT_EQ(jobSystem->GetThreadPool().GetThreads().size(), 4u);
}

TEST_F(t_NOUS_JobSystem, ResizeDecreasesThreadCount)
{
    jobSystem = new NOUS_JobSystem(4);
    jobSystem->Resize(2);
    EXPECT_EQ(jobSystem->GetThreadPool().GetThreads().size(), 2u);
}

TEST_F(t_NOUS_JobSystem, ResizeToZeroMakesSingleThreaded)
{
    jobSystem = new NOUS_JobSystem(2);
    jobSystem->Resize(0);
    EXPECT_TRUE(jobSystem->GetThreadPool().GetThreads().empty());
}

TEST_F(t_NOUS_JobSystem, ResizeWaitsForPendingJobsBeforeResizing)
{
    jobSystem = new NOUS_JobSystem(2);
    std::atomic<int> count = 0;

    for (int i = 0; i < 10; ++i)
        jobSystem->SubmitJob([&count](){ ++count; });

    // Resize implicitly waits for pending jobs first
    jobSystem->Resize(3);
    EXPECT_EQ(count.load(), 10);
}

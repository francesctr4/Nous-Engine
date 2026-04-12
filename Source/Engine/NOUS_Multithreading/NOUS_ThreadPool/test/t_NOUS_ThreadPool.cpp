#include <gtest/gtest.h>

#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include <algorithm>
#include <atomic>
#include <latch>
#include <thread>
#include <vector>

using namespace NOUS_Multithreading;

// =====================================================
// Fixture
// =====================================================

class t_NOUS_ThreadPool : public ::testing::Test
{
protected:
    static constexpr size_t kPoolSize = 32 * 1024 * 1024; // 32 MiB

    NOUS_ThreadPool* pool = nullptr;

    void SetUp() override
    {
        MemoryManager::InitializeMemory(kPoolSize);
    }

    void TearDown() override
    {
        delete pool;
        pool = nullptr;
        MemoryManager::ShutdownMemory();
    }
};

// =====================================================
// Construction
// =====================================================

TEST_F(t_NOUS_ThreadPool, ConstructorCreatesCorrectNumberOfThreads)
{
    pool = new NOUS_ThreadPool(4);
    EXPECT_EQ(pool->GetThreads().size(), 4u);
}

TEST_F(t_NOUS_ThreadPool, ConstructorWithZeroThreadsCreatesEmptyPool)
{
    pool = new NOUS_ThreadPool(0);
    EXPECT_TRUE(pool->GetThreads().empty());
}

// =====================================================
// Job execution
// =====================================================

TEST_F(t_NOUS_ThreadPool, SubmitJobExecutesJob)
{
    pool = new NOUS_ThreadPool(1);
    std::atomic<bool> ran = false;
    std::latch done(1);

    auto* job = NOUS_NEW<NOUS_Job>(MemoryTag::THREAD, "TestJob", [&ran, &done]()
    {
        ran = true;
        done.count_down();
    });
    pool->SubmitJob(job);
    done.wait();

    EXPECT_TRUE(ran);
}

TEST_F(t_NOUS_ThreadPool, MultipleJobsAllExecute)
{
    constexpr int jobCount = 10;
    pool = new NOUS_ThreadPool(2);
    std::atomic<int> count = 0;
    std::latch done(jobCount);

    for (int i = 0; i < jobCount; ++i)
    {
        auto* job = NOUS_NEW<NOUS_Job>(MemoryTag::THREAD, "Job" + std::to_string(i),
                                       [&count, &done]()
                                       {
                                           ++count;
                                           done.count_down();
                                       });
        pool->SubmitJob(job);
    }

    done.wait();
    EXPECT_EQ(count.load(), jobCount);
}

TEST_F(t_NOUS_ThreadPool, JobsRunOnWorkerThreadsNotMainThread)
{
    pool = new NOUS_ThreadPool(1);
    std::thread::id jobThreadId;
    std::latch done(1);

    auto* job = NOUS_NEW<NOUS_Job>(MemoryTag::THREAD, "ThreadIdJob", [&jobThreadId, &done]()
    {
        jobThreadId = std::this_thread::get_id();
        done.count_down();
    });
    pool->SubmitJob(job);
    done.wait();

    EXPECT_NE(jobThreadId, std::this_thread::get_id());
}

// =====================================================
// Thread IDs
// =====================================================

TEST_F(t_NOUS_ThreadPool, ThreadsHaveUniqueIDs)
{
    pool = new NOUS_ThreadPool(4);
    const auto& threads = pool->GetThreads();

    std::vector<uint32_t> ids;
    ids.reserve(threads.size());
    for (const NOUS_Thread* thread : threads)
        ids.push_back(thread->GetID());

    std::sort(ids.begin(), ids.end());
    const bool allUnique = std::adjacent_find(ids.begin(), ids.end()) == ids.end();
    EXPECT_TRUE(allUnique);
}

// =====================================================
// Snapshot
// =====================================================

TEST_F(t_NOUS_ThreadPool, GetJobQueueSnapshotIsEmptyWhenNoJobsPending)
{
    pool = new NOUS_ThreadPool(2);
    EXPECT_TRUE(pool->GetJobQueueSnapshot().empty());
}

TEST_F(t_NOUS_ThreadPool, GetJobQueueSnapshotIsEmptyAfterAllJobsComplete)
{
    constexpr int jobCount = 5;
    pool = new NOUS_ThreadPool(2);
    std::latch done(jobCount);

    for (int i = 0; i < jobCount; ++i)
    {
        auto* job = NOUS_NEW<NOUS_Job>(MemoryTag::THREAD, "Job" + std::to_string(i),
                                       [&done](){ done.count_down(); });
        pool->SubmitJob(job);
    }

    done.wait();
    EXPECT_TRUE(pool->GetJobQueueSnapshot().empty());
}

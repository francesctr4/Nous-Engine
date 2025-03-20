#pragma once

#include "NOUS_Multithreading.h"
#include "gtest/gtest.h"

void WorkerTask()
{
    std::lock_guard<std::mutex> lock(NOUS_Multithreading::sThreadsMutex);
    //std::cout << NOUS_Multithreading::GetCurrentThreadID() << std::endl;
}

// Simple test task that modifies a flag
std::atomic<bool> taskExecuted = false;
void SampleTask() {
    taskExecuted = true;
}

// Test Initialization
TEST(NOUS_MultithreadingTest, InitializeAndShutdown)
{
    NOUS_Multithreading::Initialize();

    auto mainThread = NOUS_Multithreading::GetThreadHandle(NOUS_Multithreading::GetCurrentThreadID());
    ASSERT_NE(mainThread, nullptr);
    EXPECT_EQ(mainThread->name, "Main Thread");
    EXPECT_EQ(mainThread->state, NOUS_Multithreading::ThreadState::RUNNING);

    NOUS_Multithreading::Shutdown();
}

// Test Thread Creation
TEST(NOUS_MultithreadingTest, CreateThread) {
    NOUS_Multithreading::Initialize();

    NOUS_Multithreading::NOUS_Thread* thread = NOUS_Multithreading::CreateThread("TestThread", NOUS_Multithreading::ThreadState::READY);
    ASSERT_NE(thread, nullptr);
    EXPECT_EQ(thread->name, "TestThread");
    EXPECT_EQ(thread->state, NOUS_Multithreading::ThreadState::READY);

    NOUS_Multithreading::DestroyThread(thread);
    NOUS_Multithreading::Shutdown();
}

// Test Thread Execution
TEST(NOUS_MultithreadingTest, StartThread) {
    NOUS_Multithreading::Initialize();

    taskExecuted = false;  // Reset the flag before test

    NOUS_Multithreading::NOUS_Thread* thread = NOUS_Multithreading::CreateThread("WorkerThread", NOUS_Multithreading::ThreadState::READY);
    ASSERT_NE(thread, nullptr);

    NOUS_Multithreading::StartThread(thread, SampleTask);

    if (thread->handle.joinable()) {
        thread->handle.join(); // Ensure thread finishes execution
    }

    EXPECT_EQ(thread->state, NOUS_Multithreading::ThreadState::READY);
    EXPECT_TRUE(taskExecuted);

    NOUS_Multithreading::DestroyThread(thread);
    NOUS_Multithreading::Shutdown();
}

// Test Thread State Transitions
TEST(NOUS_MultithreadingTest, ChangeState) {
    NOUS_Multithreading::Initialize();

    NOUS_Multithreading::NOUS_Thread* thread = NOUS_Multithreading::CreateThread("StateTestThread", NOUS_Multithreading::ThreadState::READY);
    ASSERT_NE(thread, nullptr);

    NOUS_Multithreading::ChangeState(thread, NOUS_Multithreading::ThreadState::RUNNING);
    EXPECT_EQ(thread->state, NOUS_Multithreading::ThreadState::RUNNING);

    NOUS_Multithreading::ChangeState(thread, NOUS_Multithreading::ThreadState::READY);
    EXPECT_EQ(thread->state, NOUS_Multithreading::ThreadState::READY);

    NOUS_Multithreading::DestroyThread(thread);
    NOUS_Multithreading::Shutdown();
}

// Test Thread Unregistration on Destruction
TEST(NOUS_MultithreadingTest, UnregisterOnDestroy) {
    NOUS_Multithreading::Initialize();

    auto* thread = NOUS_Multithreading::CreateThread("TempThread", NOUS_Multithreading::ThreadState::READY);
    uint32_t id = thread->ID;

    NOUS_Multithreading::DestroyThread(thread);

    EXPECT_THROW(
        NOUS_Multithreading::GetThreadHandle(id),
        std::out_of_range
    ); // Should be unregistered

    NOUS_Multithreading::Shutdown();
}

// Test Invalid Thread Access - EXPECTED TO FAIL (if exception not handled)
TEST(NOUS_MultithreadingTest, InvalidThreadAccess) {
    NOUS_Multithreading::Initialize();

    EXPECT_THROW(
        NOUS_Multithreading::GetThreadHandle(99999),
        std::out_of_range
    );

    NOUS_Multithreading::Shutdown();
}
// This test fails if your implementation doesn't throw exceptions for invalid access

// Test Concurrent Thread Creation
TEST(NOUS_MultithreadingTest, ConcurrentCreation) {
    NOUS_Multithreading::Initialize();
    std::vector<std::thread> creators;

    for (int i = 0; i < 10; i++) {
        creators.emplace_back([]() {
            auto* t = NOUS_Multithreading::CreateThread(
                "ConcurrentThread",
                NOUS_Multithreading::ThreadState::READY
            );
            NOUS_Multithreading::DestroyThread(t);
            });
    }

    for (auto& t : creators) t.join();

    // If mutexes work properly, this shouldn't crash
    EXPECT_TRUE(true);
    NOUS_Multithreading::Shutdown();
}

// In UnitTests.h
TEST(NOUS_MultithreadingTest, DoubleDestroy) {
    NOUS_Multithreading::Initialize();

    NOUS_Multithreading::NOUS_Thread* thread =
        NOUS_Multithreading::CreateThread("DoubleKill", NOUS_Multithreading::ThreadState::READY);

    // First destruction
    NOUS_Multithreading::DestroyThread(thread); // thread becomes nullptr

    // Second destruction (now harmless)
    EXPECT_NO_THROW(NOUS_Multithreading::DestroyThread(thread)); // Passes

    NOUS_Multithreading::Shutdown();
}

// Test 1: Destroy null pointer (should do nothing)
TEST(NOUS_MultithreadingTest, DestroyNull)
{
    NOUS_Multithreading::Initialize();

    NOUS_Multithreading::NOUS_Thread* thread = nullptr;
    EXPECT_NO_THROW(NOUS_Multithreading::DestroyThread(thread)); // No-op

    NOUS_Multithreading::Shutdown();
}

// Test 2: Access destroyed thread (should throw)
TEST(NOUS_MultithreadingTest, AccessDestroyedThread) {
    NOUS_Multithreading::Initialize();

    auto* thread = NOUS_Multithreading::CreateThread("Temp", NOUS_Multithreading::ThreadState::READY);
    uint32_t id = thread->ID;
    NOUS_Multithreading::DestroyThread(thread); // thread is now nullptr

    // Attempt to access destroyed thread
    EXPECT_THROW(
        NOUS_Multithreading::GetThreadHandle(id),
        std::out_of_range
    ); // Passes because thread was unregistered

    NOUS_Multithreading::Shutdown();
}

// Add these to UnitTests.h

// Test 1: Single Thread Lifecycle
TEST(NOUS_MultithreadingTest, SingleThreadLifecycle) {
    NOUS_Multithreading::Initialize();

    // Create thread
    auto* thread = NOUS_Multithreading::CreateThread("SingleThreadTest",
        NOUS_Multithreading::ThreadState::READY);
    ASSERT_NE(thread, nullptr);

    // Verify registration
    auto* retrievedThread = NOUS_Multithreading::GetThreadHandle(thread->ID);
    EXPECT_EQ(retrievedThread, thread);

    // Start and verify execution
    EXPECT_NO_THROW(NOUS_Multithreading::StartThread(retrievedThread, WorkerTask));

    // Wait for completion
    if (retrievedThread->handle.joinable()) {
        retrievedThread->handle.join();
    }

    // Verify final state
    EXPECT_EQ(retrievedThread->state, NOUS_Multithreading::ThreadState::READY);

    // Cleanup
    NOUS_Multithreading::UnregisterThread(thread->ID);
    NOUS_Multithreading::DestroyThread(thread);
    NOUS_Multithreading::Shutdown();
}

// Test 2: Maximum Hardware Thread Capacity
TEST(NOUS_MultithreadingTest, MaxHardwareThreads) {
    NOUS_Multithreading::Initialize();

    std::vector<NOUS_Multithreading::NOUS_Thread*> workers;

    // Create threads up to hardware limit
    for (int i = 0; i < NOUS_Multithreading::c_MAX_HARDWARE_THREADS; ++i) {
        auto* worker = NOUS_Multithreading::CreateThread(
            std::format("Worker{}", i + 1),
            NOUS_Multithreading::ThreadState::READY
        );
        ASSERT_NE(worker, nullptr);
        workers.push_back(worker);
    }

    // Start all threads
    for (auto* worker : workers) {
        EXPECT_NO_THROW(NOUS_Multithreading::StartThread(worker, WorkerTask));
    }

    // Verify and clean up
    for (auto* worker : workers) {
        if (worker->handle.joinable()) {
            worker->handle.join();
        }
        EXPECT_EQ(worker->state, NOUS_Multithreading::ThreadState::READY);
        NOUS_Multithreading::DestroyThread(worker);
    }

    NOUS_Multithreading::Shutdown();
}

// Test 3: Main Thread Validation (Edge Case)
TEST(NOUS_MultithreadingTest, MainThreadProperties) {
    NOUS_Multithreading::Initialize();

    auto mainID = NOUS_Multithreading::GetCurrentThreadID();
    auto* mainThread = NOUS_Multithreading::GetThreadHandle(mainID);

    ASSERT_NE(mainThread, nullptr);
    EXPECT_EQ(mainThread->name, "Main Thread");
    EXPECT_EQ(mainThread->state, NOUS_Multithreading::ThreadState::RUNNING);

    NOUS_Multithreading::Shutdown();
}

// Test 4: DestroyBeforeStart (Expected Failure)
TEST(NOUS_MultithreadingTest, DestroyBeforeStart)
{
    NOUS_Multithreading::Initialize();

    auto* thread = NOUS_Multithreading::CreateThread("DoomedThread",
        NOUS_Multithreading::ThreadState::READY);
    NOUS_Multithreading::DestroyThread(thread);

    EXPECT_THROW(
        NOUS_Multithreading::StartThread(thread, WorkerTask),
        std::out_of_range
    ); // EXPECTED TO FAIL if system allows starting destroyed threads

    NOUS_Multithreading::Shutdown();
}

// Test 5: Concurrent Thread Creation (Stress Test)
TEST(NOUS_MultithreadingTest, ConcurrentCreation2)
{
    NOUS_Multithreading::Initialize();
    constexpr int NUM_THREADS = 50;
    std::vector<std::thread> creators;

    // Spawn multiple creator threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        creators.emplace_back([]() {
            auto* t = NOUS_Multithreading::CreateThread("StressThread",
            NOUS_Multithreading::ThreadState::READY);
        ASSERT_NE(t, nullptr);
        NOUS_Multithreading::DestroyThread(t);
            });
    }

    // Wait for completion
    for (auto& creator : creators) {
        if (creator.joinable()) creator.join();
    }

    NOUS_Multithreading::Shutdown();

    // Verify cleanup
    EXPECT_TRUE(NOUS_Multithreading::registeredThreads.empty());
}

TEST(NOUS_MultithreadingTest, ThreadReuseAndIDConsistency) {
    NOUS_Multithreading::Initialize();

    // Create a thread in READY state
    auto* thread = NOUS_Multithreading::CreateThread("ReusableThread",
        NOUS_Multithreading::ThreadState::READY);
    ASSERT_NE(thread, nullptr);

    // Store the initial thread ID
    uint32_t initialThreadID = thread->ID;

    // First task
    NOUS_Multithreading::StartThread(thread,
        []()
        {
            std::cout << "First task executing on thread ID: "
                << NOUS_Multithreading::GetCurrentThreadID() << std::endl;
        });

    // Wait for the first task to complete
    if (thread->handle.joinable()) {
        thread->handle.join(); // Ensure the thread finishes execution
    }

    // Verify thread state after first task
    EXPECT_EQ(thread->state.load(), NOUS_Multithreading::ThreadState::READY);

    // Transition back to READY state
    NOUS_Multithreading::ChangeState(thread, NOUS_Multithreading::ThreadState::READY);
    EXPECT_EQ(thread->state.load(), NOUS_Multithreading::ThreadState::READY);

    // Verify thread ID consistency
    EXPECT_EQ(thread->ID, initialThreadID);

    // Second task
    NOUS_Multithreading::StartThread(thread,
        []()
        {
            std::cout << "Second task executing on thread ID: "
                << NOUS_Multithreading::GetCurrentThreadID() << std::endl;
        });

    // Wait for the second task to complete
    if (thread->handle.joinable()) {
        thread->handle.join(); // Ensure the thread finishes execution
    }

    // Verify thread state after second task
    EXPECT_EQ(thread->state.load(), NOUS_Multithreading::ThreadState::READY);

    // Verify thread ID consistency
    EXPECT_EQ(thread->ID, initialThreadID);

    // Clean up
    NOUS_Multithreading::DestroyThread(thread);
    NOUS_Multithreading::Shutdown();
}

#include <gtest/gtest.h>

#include <NOUS_Multithreading/NOUS_Multithreading.h>
#include <NOUS_Multithreading/NOUS_Thread.h>
#include <MemoryManager/MemoryManager.h>

#include <thread>

using namespace nous::engine::multithreading;

// =====================================================
// Fixture
// =====================================================

// RegisterMainThread allocates through NOUS_NEW, so the memory system must be up.
class t_NOUS_Multithreading : public ::testing::Test
{
protected:
    static constexpr size_t kPoolSize = 8 * 1024 * 1024; // 8 MiB

    void SetUp() override    { nous::engine::memory::InitializeMemory(kPoolSize); }
    void TearDown() override { nous::engine::memory::ShutdownMemory(); }
};

// =====================================================
// IsOnMainThread
// =====================================================

// Unit-test binaries never call RegisterMainThread(). The predicate must be
// permissive there, or NOUS_ASSERT_MAIN_THREAD fires across every ECS test.
TEST_F(t_NOUS_Multithreading, IsOnMainThreadIsTrueWhenNoMainThreadRegistered)
{
    ASSERT_EQ(GetMainThread(), nullptr);
    EXPECT_TRUE(IsOnMainThread());
}

TEST_F(t_NOUS_Multithreading, IsOnMainThreadDistinguishesThreadsWhenRegistered)
{
    RegisterMainThread();

    EXPECT_TRUE(IsOnMainThread());

    bool onOtherThread = true;
    std::thread other([&onOtherThread] { onOtherThread = IsOnMainThread(); });
    other.join();

    EXPECT_FALSE(onOtherThread);

    UnregisterMainThread();
}

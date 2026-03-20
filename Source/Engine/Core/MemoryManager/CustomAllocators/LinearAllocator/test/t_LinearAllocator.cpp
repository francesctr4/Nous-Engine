#include <gtest/gtest.h>
#include "Engine/Core/MemoryManager/CustomAllocators/LinearAllocator/include/LinearAllocator.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Fixture: initializes MemoryManager for each test (LinearAllocator uses it
// when ownsMemory = true)
// ─────────────────────────────────────────────────────────────────────────────
class t_LinearAllocator : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::InitializeMemory(65536);
    }

    void TearDown() override
    {
        MemoryManager::ShutdownMemory();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Default construction leaves allocator in empty state
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, DefaultConstructorIsEmpty)
{
    LinearAllocator la;
    EXPECT_EQ(la.GetTotalSize(),     0u);
    EXPECT_EQ(la.GetAllocatedSize(), 0u);
    EXPECT_EQ(la.GetRemainingSize(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation from owned memory
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, AllocateFromOwnedMemory)
{
    LinearAllocator la(1024);
    EXPECT_EQ(la.GetTotalSize(), 1024u);

    void* p = la.Allocate(64);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(la.GetAllocatedSize(), 64u);
    EXPECT_EQ(la.GetRemainingSize(), 960u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sequential allocations are contiguous
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, SequentialAllocationsAreContiguous)
{
    LinearAllocator la(512);
    void* a = la.Allocate(128);
    void* b = la.Allocate(128);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    // b should start immediately after a
    EXPECT_EQ(static_cast<uint8*>(b), static_cast<uint8*>(a) + 128);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocate exact capacity
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, AllocateFullCapacitySucceeds)
{
    LinearAllocator la(256);
    void* p = la.Allocate(256);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(la.GetRemainingSize(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocate beyond capacity returns null
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, AllocateBeyondCapacityReturnsNull)
{
    LinearAllocator la(128);
    void* p = la.Allocate(129);
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(la.GetAllocatedSize(), 0u); // state unchanged
}

// ─────────────────────────────────────────────────────────────────────────────
// Zero-size allocation returns null
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, AllocateZeroReturnsNull)
{
    LinearAllocator la(256);
    void* p = la.Allocate(0);
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(la.GetAllocatedSize(), 0u); // state unchanged
}

// ─────────────────────────────────────────────────────────────────────────────
// FreeAll resets cursor on owned memory
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, FreeAllResetsOwnedAllocator)
{
    LinearAllocator la(512);
    la.Allocate(256);
    EXPECT_EQ(la.GetAllocatedSize(), 256u);

    la.FreeAll();
    EXPECT_EQ(la.GetAllocatedSize(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
// FreeAll on externally-owned memory only resets the cursor (does not free)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, FreeAllOnExternalMemoryOnlyResetsCursor)
{
    constexpr uint64 kCap = 256;
    std::vector<uint8_t> buf(kCap, 0);
    LinearAllocator la(kCap, buf.data());

    la.Allocate(128);
    EXPECT_EQ(la.GetAllocatedSize(), 128u);

    la.FreeAll(); // must NOT free buf.data()
    EXPECT_EQ(la.GetAllocatedSize(), 0u);
    // If FreeAll incorrectly freed buf, the next line would crash/UB.
    EXPECT_EQ(buf[0], 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation from externally-provided buffer
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, AllocateFromExternalBuffer)
{
    constexpr uint64 kCap = 512;
    std::vector<uint8_t> buf(kCap, 0xAB);
    LinearAllocator la(kCap, buf.data());

    void* p = la.Allocate(64);
    EXPECT_EQ(p, buf.data()); // first alloc starts at buffer base
    EXPECT_EQ(la.GetAllocatedSize(), 64u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Create() re-initializes without leaking the old allocation
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_LinearAllocator, CreateCanReinitialize)
{
    LinearAllocator la(256);
    la.Allocate(64);

    // Re-initialize — old memory should be freed and offset reset
    la.Create(512);
    EXPECT_EQ(la.GetTotalSize(),     512u);
    EXPECT_EQ(la.GetAllocatedSize(), 0u);

    void* p = la.Allocate(128);
    EXPECT_NE(p, nullptr);
}

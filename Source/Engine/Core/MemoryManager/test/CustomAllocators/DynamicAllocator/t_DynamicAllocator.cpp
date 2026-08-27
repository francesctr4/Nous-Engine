#include <gtest/gtest.h>
#include "CustomAllocators/DynamicAllocator/DynamicAllocator.h"
#include <MemoryManager/MemoryManager.h>

// ─────────────────────────────────────────────────────────────────────────────
// Fixture: owns a raw malloc'd block, constructs DynamicAllocator in it
// ─────────────────────────────────────────────────────────────────────────────
class t_DynamicAllocator : public ::testing::Test
{
protected:
    static constexpr uint64 kPoolSize = 65536; // 64 KiB

    void SetUp() override
    {
        // The DynamicAllocator object (state_*) and its InternalState must live in
        // SEPARATE memory regions. Placing new(rawMemory) DA(..., rawMemory) would put
        // both at the same address: InternalState's zero-init overwrites state_ → null → crash.
        // Fix: object lives in allocator_buf (fixture member), InternalState in rawMemory.
        memReq = DynamicAllocator::GetMemoryRequirement(kPoolSize);
        rawMemory = malloc(memReq);
        allocator = new (allocator_buf) DynamicAllocator(kPoolSize, rawMemory);
    }

    void TearDown() override
    {
        allocator->~DynamicAllocator();
        free(rawMemory);
        allocator = nullptr;
        rawMemory = nullptr;
    }

    uint64            memReq    = 0;
    void*             rawMemory = nullptr;
    alignas(DynamicAllocator) char allocator_buf[sizeof(DynamicAllocator)] = {};
    DynamicAllocator* allocator = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// Initial state
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, InitialFreeSpaceEqualsPoolSize)
{
    EXPECT_EQ(allocator->GetFreeSpace(), kPoolSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// Basic allocate returns a non-null pointer
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, AllocateReturnsNonNull)
{
    void* p = allocator->Allocate(64);
    ASSERT_NE(p, nullptr);
    allocator->Free(p);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation reduces free space; free restores it
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, AllocateAndFreeTracksSpace)
{
    const uint64 before = allocator->GetFreeSpace();
    void* p = allocator->Allocate(256);
    ASSERT_NE(p, nullptr);
    EXPECT_LT(allocator->GetFreeSpace(), before);

    allocator->Free(p);
    EXPECT_EQ(allocator->GetFreeSpace(), before);
}

// ─────────────────────────────────────────────────────────────────────────────
// Multiple allocations and frees all round-trip correctly
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, MultipleAllocationsAndFrees)
{
    constexpr int N = 32;
    void* ptrs[N] = {};

    for (int i = 0; i < N; ++i)
    {
        ptrs[i] = allocator->Allocate(128);
        ASSERT_NE(ptrs[i], nullptr) << "Allocation " << i << " failed";
    }

    for (int i = 0; i < N; ++i)
        allocator->Free(ptrs[i]);

    EXPECT_EQ(allocator->GetFreeSpace(), kPoolSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pointers are distinct (no overlap)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, AllocatedPointersAreDistinct)
{
    void* a = allocator->Allocate(64);
    void* b = allocator->Allocate(64);
    EXPECT_NE(a, b);
    allocator->Free(a);
    allocator->Free(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocations are 16-byte aligned
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, AllocationsAre16ByteAligned)
{
    for (uint64 size : {1u, 7u, 15u, 17u, 63u, 128u})
    {
        void* p = allocator->Allocate(size);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u)
            << "Block of size " << size << " is not 16-byte aligned";
        allocator->Free(p);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GetRecordedSize returns original requested size (not aligned size)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, GetRecordedSizeReturnsRawSize)
{
    constexpr uint64 reqSize = 37;
    void* p = allocator->Allocate(reqSize);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(allocator->GetRecordedSize(p), reqSize);
    allocator->Free(p);
}

// ─────────────────────────────────────────────────────────────────────────────
// Size-less free (looks up size internally)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, SizelessFreeRoundtrip)
{
    const uint64 before = allocator->GetFreeSpace();
    void* p = allocator->Allocate(512);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(allocator->Free(p)); // size-less overload
    EXPECT_EQ(allocator->GetFreeSpace(), before);
}

// ─────────────────────────────────────────────────────────────────────────────
// Freeing an unknown pointer returns false (not a crash)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, FreeUnknownPointerReturnsFalse)
{
    int dummy = 42;
    EXPECT_FALSE(allocator->Free(static_cast<void*>(&dummy)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Zero-size allocation returns null
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, AllocateZeroReturnsNull)
{
    EXPECT_EQ(allocator->Allocate(0), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Freeing null is a no-op (no crash)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, FreeNullIsNoop)
{
    EXPECT_FALSE(allocator->Free(nullptr));
}

// ─────────────────────────────────────────────────────────────────────────────
// Interleaved alloc/free doesn't corrupt the pool
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_DynamicAllocator, InterleavedAllocFreeIsStable)
{
    void* a = allocator->Allocate(256);
    void* b = allocator->Allocate(256);
    allocator->Free(a);
    void* c = allocator->Allocate(128);
    allocator->Free(b);
    allocator->Free(c);

    EXPECT_EQ(allocator->GetFreeSpace(), kPoolSize);
}

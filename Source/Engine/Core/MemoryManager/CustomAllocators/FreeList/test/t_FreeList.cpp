#include <gtest/gtest.h>
#include "Engine/Core/MemoryManager/CustomAllocators/FreeList/include/FreeList.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Fixture: allocates a raw memory block for each test
// ─────────────────────────────────────────────────────────────────────────────
class t_FreeList : public ::testing::Test
{
protected:
    static constexpr uint64 kPoolSize = 4096;

    void SetUp() override
    {
        // The Freelist object (state_*) and its InternalState must live in SEPARATE
        // memory regions. Placing new(rawMemory) Freelist(..., rawMemory) would put
        // both at the same address: InternalState's zero-init overwrites state_ → null → crash.
        // Fix: object lives in freelist_buf (fixture member), InternalState in rawMemory.
        memReq = Freelist::GetMemoryRequirement(kPoolSize);
        rawMemory = malloc(memReq);
        freelist = new (freelist_buf) Freelist(kPoolSize, rawMemory);
    }

    void TearDown() override
    {
        freelist->~Freelist();
        free(rawMemory);
        freelist = nullptr;
        rawMemory = nullptr;
    }

    uint64   memReq   = 0;
    void*    rawMemory = nullptr;
    alignas(Freelist) char freelist_buf[sizeof(Freelist)] = {};
    Freelist* freelist = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// Initial state
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, InitialFreeSpaceEqualsPoolSize)
{
    EXPECT_EQ(freelist->FreeSpace(), kPoolSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// Basic allocation reduces free space
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, AllocateReducesFreeSpace)
{
    uint64 offset = 0;
    ASSERT_TRUE(freelist->Allocate(256, &offset));
    EXPECT_EQ(freelist->FreeSpace(), kPoolSize - 256);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation returns sequential offsets
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, SequentialAllocationsReturnSequentialOffsets)
{
    uint64 off1 = 0, off2 = 0;
    ASSERT_TRUE(freelist->Allocate(128, &off1));
    ASSERT_TRUE(freelist->Allocate(128, &off2));
    EXPECT_EQ(off1, 0u);
    EXPECT_EQ(off2, 128u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Exact-fit allocation
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, ExactFitAllocationDrainsPool)
{
    uint64 offset = 0;
    ASSERT_TRUE(freelist->Allocate(kPoolSize, &offset));
    EXPECT_EQ(freelist->FreeSpace(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Free restores space
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, FreeRestoresFreeSpace)
{
    uint64 offset = 0;
    ASSERT_TRUE(freelist->Allocate(256, &offset));
    ASSERT_TRUE(freelist->Free(256, offset));
    EXPECT_EQ(freelist->FreeSpace(), kPoolSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// Adjacent freed blocks are merged
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, AdjacentFreeBlocksMerge)
{
    uint64 off1 = 0, off2 = 0;
    ASSERT_TRUE(freelist->Allocate(256, &off1));
    ASSERT_TRUE(freelist->Allocate(256, &off2));
    ASSERT_TRUE(freelist->Free(256, off1));
    ASSERT_TRUE(freelist->Free(256, off2));
    // After merging, a single 512-byte block should be allocatable
    uint64 bigOffset = 0;
    EXPECT_TRUE(freelist->Allocate(512, &bigOffset));
}

// ─────────────────────────────────────────────────────────────────────────────
// Free block that falls AFTER all existing free nodes (was the original bug)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, FreeBlockAfterAllExistingFreeNodes)
{
    // Drain the entire pool so the only free space comes from explicit frees.
    // This makes FreeSpace() == sum-of-freed-blocks, which is what we want to verify.
    constexpr uint64 chunkSize = 128;
    constexpr uint64 numChunks = kPoolSize / chunkSize; // 32
    uint64 offsets[numChunks] = {};
    for (uint64 i = 0; i < numChunks; ++i)
        ASSERT_TRUE(freelist->Allocate(chunkSize, &offsets[i]));
    ASSERT_EQ(freelist->FreeSpace(), 0u);

    // Free the first block — becomes the only free node at offset 0
    ASSERT_TRUE(freelist->Free(chunkSize, offsets[0]));

    // Free the third block — its offset is AFTER the only free node.
    // This previously fell through without inserting, silently leaking the block.
    ASSERT_TRUE(freelist->Free(chunkSize, offsets[2]));

    EXPECT_EQ(freelist->FreeSpace(), 256u); // offsets[0](128) + offsets[2](128)
}

// ─────────────────────────────────────────────────────────────────────────────
// Out-of-memory returns false (not a crash)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, AllocateBeyondCapacityReturnsFalse)
{
    uint64 offset = 0;
    EXPECT_FALSE(freelist->Allocate(kPoolSize + 1, &offset));
}

// ─────────────────────────────────────────────────────────────────────────────
// Zero-size allocation returns false
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, AllocateZeroSizeReturnsFalse)
{
    uint64 offset = 0;
    EXPECT_FALSE(freelist->Allocate(0, &offset));
}

// ─────────────────────────────────────────────────────────────────────────────
// Out-of-bounds free returns false
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, FreeOutOfBoundsReturnsFalse)
{
    EXPECT_FALSE(freelist->Free(128, kPoolSize)); // offset 4096 + size 128 > 4096
}

// ─────────────────────────────────────────────────────────────────────────────
// Tiny pool (smaller than one Node — triggers the min-1-node fix)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, TinyPoolAllocationAndFreeRoundtrip)
{
    constexpr uint64 tinySize = 4;
    uint64 req = Freelist::GetMemoryRequirement(tinySize);
    void* mem = malloc(req);
    alignas(Freelist) char tiny_buf[sizeof(Freelist)] = {};
    Freelist* tiny = new (tiny_buf) Freelist(tinySize, mem);

    uint64 offset = 0;
    EXPECT_TRUE(tiny->Allocate(tinySize, &offset));
    EXPECT_EQ(offset, 0u);
    EXPECT_EQ(tiny->FreeSpace(), 0u);
    EXPECT_TRUE(tiny->Free(tinySize, offset));
    EXPECT_EQ(tiny->FreeSpace(), tinySize);

    tiny->~Freelist();
    free(mem);
}

// ─────────────────────────────────────────────────────────────────────────────
// Clear resets to full capacity and leaves no dangling list
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, ClearRestoresFullCapacity)
{
    uint64 off1 = 0, off2 = 0;
    ASSERT_TRUE(freelist->Allocate(512, &off1));
    ASSERT_TRUE(freelist->Allocate(512, &off2));
    // Free in reverse order so two separate nodes exist
    ASSERT_TRUE(freelist->Free(512, off2));
    ASSERT_TRUE(freelist->Free(512, off1));

    freelist->Clear();
    EXPECT_EQ(freelist->FreeSpace(), kPoolSize);

    // Should be able to allocate full capacity again
    uint64 bigOffset = 0;
    EXPECT_TRUE(freelist->Allocate(kPoolSize, &bigOffset));
}

// ─────────────────────────────────────────────────────────────────────────────
// Node slot is not permanently lost when nodes[0] is returned
// (tests the GetNode() index-0 fix)
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(t_FreeList, NodeZeroIsReusableAfterExactFitAllocation)
{
    // Exact-fit allocation removes nodes[0] (the head) and returns it.
    // GetNode() must be able to reclaim it for the next Free call.
    uint64 offset = 0;
    ASSERT_TRUE(freelist->Allocate(kPoolSize, &offset));
    EXPECT_EQ(freelist->FreeSpace(), 0u);

    // Free the whole pool — requires GetNode to find a slot (including nodes[0])
    ASSERT_TRUE(freelist->Free(kPoolSize, offset));
    EXPECT_EQ(freelist->FreeSpace(), kPoolSize);

    // Subsequent alloc/free cycle must still work
    ASSERT_TRUE(freelist->Allocate(kPoolSize, &offset));
    ASSERT_TRUE(freelist->Free(kPoolSize, offset));
    EXPECT_EQ(freelist->FreeSpace(), kPoolSize);
}

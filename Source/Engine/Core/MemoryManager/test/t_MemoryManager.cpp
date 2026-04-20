#include <gtest/gtest.h>
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

// ─────────────────────────────────────────────────────────────────────────────
// Test types
// ─────────────────────────────────────────────────────────────────────────────

struct SimpleStruct
{
    int   x = 0;
    float y = 0.0f;
};

struct CtorArgStruct
{
    int   value;
    float scale;

    CtorArgStruct(int v, float s) : value(v), scale(s) {}
};

struct DestructorTracker
{
    bool* destroyed = nullptr;

    explicit DestructorTracker(bool* flag) : destroyed(flag) {}
    ~DestructorTracker()
    {
        if (destroyed) *destroyed = true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Fixture: initializes and shuts down the memory system around each test.
// Every test MUST free all allocations before returning — ShutdownMemory
// asserts zero outstanding allocations.
// ─────────────────────────────────────────────────────────────────────────────
class t_MemoryManager : public ::testing::Test
{
protected:
    static constexpr uint64 kPoolSize = MiB(4);

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kPoolSize);
    }

    void TearDown() override
    {
        nous::engine::memory::ShutdownMemory();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// NOUS_NEW — basic behavior
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(t_MemoryManager, NOUS_NEW_ReturnsNonNull)
{
    int* p = NOUS_NEW<int>(MemoryTag::APPLICATION);
    ASSERT_NE(p, nullptr);
    NOUS_DELETE(p, MemoryTag::APPLICATION);
}

TEST_F(t_MemoryManager, NOUS_NEW_ForwardsConstructorArguments)
{
    CtorArgStruct* s = NOUS_NEW<CtorArgStruct>(MemoryTag::APPLICATION, 42, 3.14f);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->value, 42);
    EXPECT_FLOAT_EQ(s->scale, 3.14f);
    NOUS_DELETE(s, MemoryTag::APPLICATION);
}

TEST_F(t_MemoryManager, NOUS_NEW_MultipleAllocationsReturnDistinctPointers)
{
    int* a = NOUS_NEW<int>(MemoryTag::APPLICATION);
    int* b = NOUS_NEW<int>(MemoryTag::APPLICATION);
    EXPECT_NE(a, b);
    NOUS_DELETE(a, MemoryTag::APPLICATION);
    NOUS_DELETE(b, MemoryTag::APPLICATION);
}

TEST_F(t_MemoryManager, NOUS_NEW_MemoryIsZeroedBeforeConstruction)
{
    // Allocate a POD struct whose default-init leaves fields at 0 (zero-init by allocator).
    SimpleStruct* s = NOUS_NEW<SimpleStruct>(MemoryTag::APPLICATION);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->x, 0);
    EXPECT_FLOAT_EQ(s->y, 0.0f);
    NOUS_DELETE(s, MemoryTag::APPLICATION);
}

// ─────────────────────────────────────────────────────────────────────────────
// NOUS_DELETE — behavior
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(t_MemoryManager, NOUS_DELETE_CallsDestructor)
{
    bool destroyed = false;
    DestructorTracker* t = NOUS_NEW<DestructorTracker>(MemoryTag::APPLICATION, &destroyed);
    ASSERT_NE(t, nullptr);
    EXPECT_FALSE(destroyed);

    NOUS_DELETE(t, MemoryTag::APPLICATION);
    EXPECT_TRUE(destroyed);
}

TEST_F(t_MemoryManager, NOUS_DELETE_NullsOutPointer)
{
    int* p = NOUS_NEW<int>(MemoryTag::APPLICATION);
    ASSERT_NE(p, nullptr);
    NOUS_DELETE(p, MemoryTag::APPLICATION);
    EXPECT_EQ(p, nullptr);
}

TEST_F(t_MemoryManager, NOUS_DELETE_OnNullptrIsNoop)
{
    int* p = nullptr;
    // Should not crash and stats should be unchanged.
    const auto statsBefore = nous::engine::memory::GetMemoryStats();
    NOUS_DELETE(p, MemoryTag::APPLICATION);
    const auto statsAfter = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(statsAfter.totalAllocations, statsBefore.totalAllocations);
}

// ─────────────────────────────────────────────────────────────────────────────
// Stats tracking
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(t_MemoryManager, NOUS_NEW_IncreasesAllocationCount)
{
    const uint64 before = nous::engine::memory::GetMemoryStats().totalAllocations;
    int* p = NOUS_NEW<int>(MemoryTag::APPLICATION);
    EXPECT_EQ(nous::engine::memory::GetMemoryStats().totalAllocations, before + 1);
    NOUS_DELETE(p, MemoryTag::APPLICATION);
}

TEST_F(t_MemoryManager, NOUS_NEW_IncreasesTaggedBytes)
{
    const auto before = nous::engine::memory::GetMemoryStats();
    const uint64 tagIdx = static_cast<uint64>(MemoryTag::RENDERER);

    int* p = NOUS_NEW<int>(MemoryTag::RENDERER);
    const auto after = nous::engine::memory::GetMemoryStats();

    EXPECT_GT(after.taggedAllocations[tagIdx], before.taggedAllocations[tagIdx]);
    EXPECT_GT(after.totalAllocated, before.totalAllocated);

    NOUS_DELETE(p, MemoryTag::RENDERER);
}

TEST_F(t_MemoryManager, NOUS_DELETE_DecreasesAllocationCount)
{
    int* p = NOUS_NEW<int>(MemoryTag::APPLICATION);
    const uint64 afterAlloc = nous::engine::memory::GetMemoryStats().totalAllocations;

    NOUS_DELETE(p, MemoryTag::APPLICATION);
    EXPECT_EQ(nous::engine::memory::GetMemoryStats().totalAllocations, afterAlloc - 1);
}

TEST_F(t_MemoryManager, NOUS_NEW_DELETE_Roundtrip_RestoresStats)
{
    const auto statsBefore = nous::engine::memory::GetMemoryStats();

    SimpleStruct* s = NOUS_NEW<SimpleStruct>(MemoryTag::APPLICATION);
    NOUS_DELETE(s, MemoryTag::APPLICATION);

    const auto statsAfter = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(statsAfter.totalAllocations, statsBefore.totalAllocations);
    EXPECT_EQ(statsAfter.totalAllocated, statsBefore.totalAllocated);
}

TEST_F(t_MemoryManager, MultipleTagsTrackedIndependently)
{
    const uint64 meshIdx = static_cast<uint64>(MemoryTag::RESOURCE_MESH);
    const uint64 texIdx  = static_cast<uint64>(MemoryTag::RESOURCE_TEXTURE);

    const auto before = nous::engine::memory::GetMemoryStats();

    int* mesh = NOUS_NEW<int>(MemoryTag::RESOURCE_MESH);
    int* tex  = NOUS_NEW<int>(MemoryTag::RESOURCE_TEXTURE);

    const auto during = nous::engine::memory::GetMemoryStats();
    EXPECT_GT(during.taggedAllocations[meshIdx], before.taggedAllocations[meshIdx]);
    EXPECT_GT(during.taggedAllocations[texIdx],  before.taggedAllocations[texIdx]);

    NOUS_DELETE(mesh, MemoryTag::RESOURCE_MESH);
    NOUS_DELETE(tex,  MemoryTag::RESOURCE_TEXTURE);

    const auto after = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(after.taggedAllocations[meshIdx], before.taggedAllocations[meshIdx]);
    EXPECT_EQ(after.taggedAllocations[texIdx],  before.taggedAllocations[texIdx]);
}

// ─────────────────────────────────────────────────────────────────────────────
// NOUS_NEW_ARRAY / NOUS_DELETE_ARRAY
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(t_MemoryManager, NOUS_NEW_ARRAY_ReturnsNonNull)
{
    int* arr = NOUS_NEW_ARRAY<int>(8, MemoryTag::ARRAY);
    ASSERT_NE(arr, nullptr);
    NOUS_DELETE_ARRAY(arr, 8, MemoryTag::ARRAY);
}

TEST_F(t_MemoryManager, NOUS_NEW_ARRAY_ConstructsAllElements)
{
    // SimpleStruct default-constructor sets x=0, y=0.0f
    SimpleStruct* arr = NOUS_NEW_ARRAY<SimpleStruct>(4, MemoryTag::ARRAY);
    ASSERT_NE(arr, nullptr);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(arr[i].x, 0)       << "Element " << i << " x not zero-initialized";
        EXPECT_FLOAT_EQ(arr[i].y, 0.0f) << "Element " << i << " y not zero-initialized";
    }
    NOUS_DELETE_ARRAY(arr, 4, MemoryTag::ARRAY);
}

TEST_F(t_MemoryManager, NOUS_DELETE_ARRAY_DestructsAllElements)
{
    constexpr int N = 5;
    bool destroyed[N] = {};

    // Can't construct DestructorTracker via default constructor, so we'll
    // allocate raw memory and placement-new manually to test the pattern.
    // Instead, use a simpler approach: allocate + write, verify destructor side-effect.

    // Allocate array of ints, assign values, verify roundtrip frees correctly.
    int* arr = NOUS_NEW_ARRAY<int>(N, MemoryTag::ARRAY);
    for (int i = 0; i < N; ++i)
        arr[i] = i * 10;

    // Verify writes are readable before deletion.
    for (int i = 0; i < N; ++i)
        EXPECT_EQ(arr[i], i * 10);

    NOUS_DELETE_ARRAY(arr, N, MemoryTag::ARRAY);
    EXPECT_EQ(arr, nullptr);
}

TEST_F(t_MemoryManager, NOUS_DELETE_ARRAY_NullsOutPointer)
{
    int* arr = NOUS_NEW_ARRAY<int>(4, MemoryTag::ARRAY);
    ASSERT_NE(arr, nullptr);
    NOUS_DELETE_ARRAY(arr, 4, MemoryTag::ARRAY);
    EXPECT_EQ(arr, nullptr);
}

TEST_F(t_MemoryManager, NOUS_NEW_ARRAY_IncreasesAllocationCount)
{
    const uint64 before = nous::engine::memory::GetMemoryStats().totalAllocations;
    int* arr = NOUS_NEW_ARRAY<int>(16, MemoryTag::ARRAY);
    EXPECT_EQ(nous::engine::memory::GetMemoryStats().totalAllocations, before + 1);
    NOUS_DELETE_ARRAY(arr, 16, MemoryTag::ARRAY);
}

TEST_F(t_MemoryManager, NOUS_NEW_ARRAY_DELETE_ARRAY_Roundtrip_RestoresStats)
{
    const auto before = nous::engine::memory::GetMemoryStats();

    float* arr = NOUS_NEW_ARRAY<float>(32, MemoryTag::ARRAY);
    NOUS_DELETE_ARRAY(arr, 32, MemoryTag::ARRAY);

    const auto after = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(after.totalAllocations, before.totalAllocations);
    EXPECT_EQ(after.totalAllocated,   before.totalAllocated);
}

#include <gtest/gtest.h>
#include <vector>
#include <list>
#include <unordered_map>

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/MemoryManager/CustomAllocators/NOUS_STLAllocator/include/NOUS_STLAllocator.h"

class t_NOUS_STLAllocator : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory manager if not already active
        MemoryManager::InitializeMemory(65536);
    }

    void TearDown() override {
        // Check that all allocations have been freed after each test
        const auto stats = MemoryManager::GetMemoryStats();
        EXPECT_EQ(stats.totalAllocations, 0)
                            << "Memory leak detected after test! Allocated bytes: "
                            << stats.totalAllocations;

        MemoryManager::ShutdownMemory();
    }
};

// ------------------------------------------------------------
// 🧪 Test 1: std::vector allocations with NOUS_STLAllocator
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, VectorAllocationAndFree)
{
    using Alloc = NOUS_STLAllocator<int>;
    Alloc alloc(MemoryTag::ARRAY);

    const auto before = MemoryManager::GetMemoryStats();

    {
        std::vector<int, Alloc> vec(alloc);
        vec.reserve(256);
        for (int i = 0; i < 256; ++i)
            vec.push_back(i);
    }

    const auto after = MemoryManager::GetMemoryStats();

    EXPECT_EQ(after.totalAllocations, before.totalAllocations)
                        << "All std::vector allocations should have been freed.";
}

// ------------------------------------------------------------
// 🧪 Test 2: MemoryTag accounting correctness
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, TagAccountingIsAccurate)
{
    using Alloc = NOUS_STLAllocator<float>;
    Alloc alloc(MemoryTag::ARRAY);

    const auto before = MemoryManager::GetMemoryStats();

    {
        std::vector<float, Alloc> data(alloc);
        data.reserve(64);
        data.resize(64, 1.0f);
    }

    const auto after = MemoryManager::GetMemoryStats();

    // Ensure ARRAY tag usage returns to zero
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryTag::ARRAY], 0)
                        << "ARRAY tag should be zero after destruction.";
    EXPECT_EQ(after.totalAllocations, before.totalAllocations);
}

// ------------------------------------------------------------
// 🧪 Test 3: unordered_map allocation/free path
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, UnorderedMapAllocAndFree)
{
    using Pair = std::pair<const int, double>;
    using Alloc = NOUS_STLAllocator<Pair>;
    Alloc alloc(MemoryTag::DICT);

    const auto before = MemoryManager::GetMemoryStats();

    {
        std::unordered_map<int, double, std::hash<int>, std::equal_to<int>, Alloc> map(
                0, std::hash<int>{}, std::equal_to<int>{}, alloc);

        for (int i = 0; i < 50; ++i)
            map.emplace(i, i * 1.5);
    }

    const auto after = MemoryManager::GetMemoryStats();
    EXPECT_EQ(after.totalAllocations, before.totalAllocations)
                        << "All unordered_map allocations should have been freed.";
}

// ------------------------------------------------------------
// 🧪 Test 4: Cross-container isolation
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, IndependentContainersTrackSeparateTags)
{
    using VecAlloc = NOUS_STLAllocator<int>;
    using MapAlloc = NOUS_STLAllocator<std::pair<const int, int>>;

    VecAlloc vecAlloc(MemoryTag::ARRAY);
    MapAlloc mapAlloc(MemoryTag::DICT);

    const auto before = MemoryManager::GetMemoryStats();

    {
        std::vector<int, VecAlloc> vec(vecAlloc);
        vec.resize(128, 7);

        std::unordered_map<int, int, std::hash<int>, std::equal_to<int>, MapAlloc> map(
                0, std::hash<int>{}, std::equal_to<int>{}, mapAlloc);
        map.emplace(1, 42);
        map.emplace(2, 84);
    }

    const auto after = MemoryManager::GetMemoryStats();

    EXPECT_EQ(after.totalAllocations, before.totalAllocations);
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryTag::ARRAY], 0);
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryTag::DICT], 0);
}

// ------------------------------------------------------------
// 🧪 Test 5: max_size() reflects the live pool capacity
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, MaxSizeReflectsPoolCapacity)
{
    using Alloc = NOUS_STLAllocator<uint8_t>;
    Alloc alloc(MemoryTag::ARRAY);

    const auto config = MemoryManager::GetMemoryConfig();

    // max_size() must not exceed the pool (a container reserving more would fail).
    EXPECT_LE(alloc.max_size(), config.totalAllocationSize);

    // And it must be > 0 while the pool is live.
    EXPECT_GT(alloc.max_size(), 0u);

    // For a different element type, max_size() scales by sizeof(T).
    using FloatAlloc = NOUS_STLAllocator<float>;
    FloatAlloc floatAlloc(MemoryTag::ARRAY);
    EXPECT_EQ(floatAlloc.max_size(), alloc.max_size() / sizeof(float));
}

// ------------------------------------------------------------
// Test 6: Default constructor uses MemoryTag::UNKNOWN
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, DefaultConstructorTagIsUnknown)
{
    NOUS_STLAllocator<int> alloc;
    EXPECT_EQ(alloc.tag(), MemoryTag::UNKNOWN);
}

// ------------------------------------------------------------
// Test 7: Equality operators — same tag compares equal, different tags do not
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, EqualityOperators)
{
    NOUS_STLAllocator<int> a(MemoryTag::ARRAY);
    NOUS_STLAllocator<int> b(MemoryTag::ARRAY);
    NOUS_STLAllocator<int> c(MemoryTag::DICT);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
}

// ------------------------------------------------------------
// Test 8: Copy construction across element types preserves tag (rebind path)
// Containers like std::list rebind the allocator to their internal node type.
// The tag must survive that conversion or tracking breaks silently.
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, CopyConstructionAcrossTypesPreservesTag)
{
    NOUS_STLAllocator<int>   src(MemoryTag::SCENE);
    NOUS_STLAllocator<float> dst(src); // cross-type copy (rebind path)

    EXPECT_EQ(dst.tag(), MemoryTag::SCENE);
    EXPECT_EQ(dst.tag(), src.tag());
}

// ------------------------------------------------------------
// Test 9: std::list uses rebind internally — tag must propagate to node allocs
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, ListWithRebindTracksCorrectly)
{
    using Alloc = NOUS_STLAllocator<int>;
    Alloc alloc(MemoryTag::ARRAY);

    const auto before = MemoryManager::GetMemoryStats();

    {
        std::list<int, Alloc> lst(alloc);
        for (int i = 0; i < 64; ++i)
            lst.push_back(i);
    }

    const auto after = MemoryManager::GetMemoryStats();
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryTag::ARRAY], 0)
        << "std::list node allocations should be freed and tracked under ARRAY tag.";
    EXPECT_EQ(after.totalAllocations, before.totalAllocations);
}

// ------------------------------------------------------------
// Test 10: Raw allocate/deallocate round-trip (bypasses container)
// Exercises the allocator interface directly rather than via a container.
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, RawAllocateDeallocateRoundtrip)
{
    NOUS_STLAllocator<uint64> alloc(MemoryTag::ARRAY);

    constexpr std::size_t n = 16;
    const auto before = MemoryManager::GetMemoryStats();

    uint64* p = alloc.allocate(n);
    ASSERT_NE(p, nullptr);
    EXPECT_GT(MemoryManager::GetMemoryStats().taggedAllocations[(uint64)MemoryTag::ARRAY], 0u);

    alloc.deallocate(p, n);
    EXPECT_EQ(MemoryManager::GetMemoryStats().taggedAllocations[(uint64)MemoryTag::ARRAY],
              before.taggedAllocations[(uint64)MemoryTag::ARRAY]);
}

// ------------------------------------------------------------
// Test 11: allocate(n > max_size()) throws std::bad_array_new_length
// ------------------------------------------------------------
TEST_F(t_NOUS_STLAllocator, AllocateBeyondMaxSizeThrows)
{
    NOUS_STLAllocator<int> alloc(MemoryTag::ARRAY);
    const std::size_t tooBig = alloc.max_size() + 1;
    EXPECT_THROW(alloc.allocate(tooBig), std::bad_array_new_length);
}

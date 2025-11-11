#include <gtest/gtest.h>
#include <vector>
#include <unordered_map>

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/MemoryManager/CustomAllocators/NOUS_STLAllocator/include/NOUS_STLAllocator.h"

class t_NOUS_STLAllocator : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory manager if not already active
        MemoryManager::InitializeMemory(300);
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
    Alloc alloc(MemoryManager::MemoryTag::ARRAY);

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
    Alloc alloc(MemoryManager::MemoryTag::ARRAY);

    const auto before = MemoryManager::GetMemoryStats();

    {
        std::vector<float, Alloc> data(alloc);
        data.reserve(64);
        data.resize(64, 1.0f);
    }

    const auto after = MemoryManager::GetMemoryStats();

    // Ensure ARRAY tag usage returns to zero
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryManager::MemoryTag::ARRAY], 0)
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
    Alloc alloc(MemoryManager::MemoryTag::DICT);

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

    VecAlloc vecAlloc(MemoryManager::MemoryTag::ARRAY);
    MapAlloc mapAlloc(MemoryManager::MemoryTag::DICT);

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
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryManager::MemoryTag::ARRAY], 0);
    EXPECT_EQ(after.taggedAllocations[(uint64)MemoryManager::MemoryTag::DICT], 0);
}

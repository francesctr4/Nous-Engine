#include <gtest/gtest.h>

#include <MemoryManager/MemoryManager.h>
#include <Utils/DataStructures/NOUS_Vector.h>

#include <cstdint>
#include <cstring>
#include <string>

class t_NOUS_Vector : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(4));
    }

    void TearDown() override
    {
        const auto stats = nous::engine::memory::GetMemoryStats();
        EXPECT_EQ(stats.totalAllocations, 0)
            << "Memory leak detected after test! Allocated bytes: " << stats.totalAllocations;

        nous::engine::memory::ShutdownMemory();
    }
};

// =============================================================================
// data() / resize() / reserve() / assign() — the methods Task 2 depends on
// =============================================================================

TEST_F(t_NOUS_Vector, Resize_ThenData_GivesWritableContiguousBuffer)
{
    NOUS_Vector<char> buffer(MemoryTag::FILE);
    buffer.resize(8);

    ASSERT_EQ(buffer.size(), 8u);
    ASSERT_NE(buffer.data(), nullptr);

    // The whole point of data(): hand the raw pointer to a C API that fills it.
    const char payload[8] = { 'N', 'o', 'u', 's', 'E', 'n', 'g', 'n' };
    std::memcpy(buffer.data(), payload, sizeof(payload));

    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "NousEngn");
}

TEST_F(t_NOUS_Vector, Data_OnEmptyVector_IsSafeToCall)
{
    NOUS_Vector<char> buffer(MemoryTag::FILE);
    EXPECT_EQ(buffer.size(), 0u);
    // Must not crash. A null return is acceptable; the contract is only that
    // calling data() on an empty vector is defined.
    (void)buffer.data();
}

TEST_F(t_NOUS_Vector, ConstData_ReadsBackWhatWasWritten)
{
    NOUS_Vector<int> values(MemoryTag::ARRAY);
    values.resize(3);
    values[0] = 10;
    values[1] = 20;
    values[2] = 30;

    const NOUS_Vector<int>& constRef = values;
    ASSERT_NE(constRef.data(), nullptr);
    EXPECT_EQ(constRef.data()[0], 10);
    EXPECT_EQ(constRef.data()[2], 30);
}

TEST_F(t_NOUS_Vector, Reserve_DoesNotChangeSize)
{
    NOUS_Vector<char> buffer(MemoryTag::FILE);
    buffer.reserve(128);
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.empty());
}

TEST_F(t_NOUS_Vector, Resize_Shrinking_TruncatesToNewSize)
{
    NOUS_Vector<char> buffer(MemoryTag::FILE);
    buffer.resize(16);
    std::memset(buffer.data(), 'x', 16);

    // Task 2 relies on this: a short read sizes the vector down to bytes
    // actually read, so length and buffer can never disagree.
    buffer.resize(4);

    EXPECT_EQ(buffer.size(), 4u);
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "xxxx");
}

TEST_F(t_NOUS_Vector, Assign_FromIteratorPair_CopiesRange)
{
    const std::string payload = "shader source";

    NOUS_Vector<char> buffer(MemoryTag::FILE);
    buffer.assign(payload.begin(), payload.end());

    EXPECT_EQ(buffer.size(), payload.size());
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), payload);
}

// =============================================================================
// Tag propagation — the reason this wrapper exists instead of std::vector
// =============================================================================

TEST_F(t_NOUS_Vector, ChargesItsTag_AndReleasesOnDestruction)
{
    const auto before = nous::engine::memory::GetMemoryStats();
    const uint64_t fileTag = static_cast<uint64_t>(MemoryTag::FILE);

    {
        NOUS_Vector<char> buffer(MemoryTag::FILE);
        buffer.resize(1024);

        const auto during = nous::engine::memory::GetMemoryStats();
        EXPECT_GT(during.taggedAllocations[fileTag], before.taggedAllocations[fileTag])
            << "A NOUS_Vector<char>(MemoryTag::FILE) must charge the FILE tag. "
               "If this fails, file reads are invisible in MemoryWindow.";
    }

    const auto after = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(after.taggedAllocations[fileTag], before.taggedAllocations[fileTag])
        << "FILE tag must return to its starting value after destruction.";
    EXPECT_EQ(after.totalAllocations, before.totalAllocations);
}

TEST_F(t_NOUS_Vector, DefaultConstructed_UsesUnknownTag)
{
    const auto before = nous::engine::memory::GetMemoryStats();
    const uint64_t unknownTag = static_cast<uint64_t>(MemoryTag::UNKNOWN);

    {
        NOUS_Vector<char> buffer;
        buffer.resize(256);

        const auto during = nous::engine::memory::GetMemoryStats();
        EXPECT_GT(during.taggedAllocations[unknownTag], before.taggedAllocations[unknownTag]);
    }

    const auto after = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(after.taggedAllocations[unknownTag], before.taggedAllocations[unknownTag]);
}

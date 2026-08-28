#include <gtest/gtest.h>

#include <Utils/Serialization/Random.h>

#include <unordered_set>
#include <cstddef>

// =============================================================================
// Generate
// =============================================================================

TEST(t_Random, Generate_DoesNotCrash)
{
    EXPECT_NO_THROW(Random::Generate());
}

TEST(t_Random, Generate_MultipleCallsProduceDifferentValues)
{
    // With 64-bit output space, the probability of a collision in 100 calls
    // is astronomically small (~5e-13). Any failure here indicates a broken RNG.
    constexpr int kCount = 100;
    std::unordered_set<uint64_t> seen;
    for (int i = 0; i < kCount; ++i)
        seen.insert(Random::Generate());
    EXPECT_GT(seen.size(), static_cast<size_t>(kCount / 2));
}

// =============================================================================
// GenerateInterval
// =============================================================================

TEST(t_Random, GenerateInterval_SingleValue_ReturnsExactly)
{
    EXPECT_EQ(Random::GenerateInterval(42u, 42u), 42u);
}

TEST(t_Random, GenerateInterval_Result_IsWithinBounds)
{
    constexpr uint64_t lo = 10u;
    constexpr uint64_t hi = 20u;
    for (int i = 0; i < 200; ++i)
    {
        const uint64_t v = Random::GenerateInterval(lo, hi);
        EXPECT_GE(v, lo);
        EXPECT_LE(v, hi);
    }
}

TEST(t_Random, GenerateInterval_ZeroToZero_ReturnsZero)
{
    EXPECT_EQ(Random::GenerateInterval(0u, 0u), 0u);
}

TEST(t_Random, GenerateInterval_ProducesVariety)
{
    // With range [0, 1000] and 500 samples, expect multiple distinct values
    constexpr int kSamples = 500;
    std::unordered_set<uint64_t> seen;
    for (int i = 0; i < kSamples; ++i)
        seen.insert(Random::GenerateInterval(0u, 1000u));
    EXPECT_GT(seen.size(), 50u);
}

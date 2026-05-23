#include <gtest/gtest.h>

#include "Engine/Renderer/Backend/Vulkan/Resources/Shader/VulkanShaderDescriptorState.h"

#include <cstdint>
#include <limits>

// =============================================================================
// VulkanShaderDescriptorState — struct initialization invariants
// =============================================================================

TEST(VulkanShaderDescriptorState, Generations_InitializeToUINT32MAX)
{
    VulkanShaderDescriptorState state;
    for (const uint32_t g : state.generations)
        EXPECT_EQ(g, UINT32_MAX);
}

TEST(VulkanShaderDescriptorState, IDs_InitializeToUINT32MAX)
{
    VulkanShaderDescriptorState state;
    for (const uint32_t id : state.ids)
        EXPECT_EQ(id, UINT32_MAX);
}

// =============================================================================
// SamplerNeedsWrite — write-decision logic
// =============================================================================

TEST(SamplerNeedsWrite, NeverWritten_AlwaysTrue)
{
    // UINT32_MAX is the sentinel for "never written".
    // Must return true regardless of resource ID/generation values.
    EXPECT_TRUE(SamplerNeedsWrite(UINT32_MAX, 0u, 42u, 7u));
    EXPECT_TRUE(SamplerNeedsWrite(UINT32_MAX, 42u, 42u, 7u));  // ID matches — still true
    EXPECT_TRUE(SamplerNeedsWrite(UINT32_MAX, 7u,  42u, 7u));  // gen matches — still true
}

TEST(SamplerNeedsWrite, SameResource_ReturnsFalse)
{
    // After a write, state holds (generation=5, id=10).
    // Same resource presented again — no write needed.
    EXPECT_FALSE(SamplerNeedsWrite(5u, 10u, 10u, 5u));
}

TEST(SamplerNeedsWrite, DifferentID_ReturnsTrue)
{
    // State holds (generation=5, id=10). Resource changed to id=99 — must write.
    EXPECT_TRUE(SamplerNeedsWrite(5u, 10u, 99u, 5u));
}

TEST(SamplerNeedsWrite, DifferentGeneration_ReturnsTrue)
{
    // Hot-reload case: same resource ID but generation bumped.
    // State holds (generation=1, id=10). Resource now at generation=2.
    EXPECT_TRUE(SamplerNeedsWrite(1u, 10u, 10u, 2u));
}

TEST(SamplerNeedsWrite, BothDiffer_ReturnsTrue)
{
    // Both ID and generation changed — must write.
    EXPECT_TRUE(SamplerNeedsWrite(1u, 10u, 99u, 2u));
}

// =============================================================================
// SamplerNeedsWrite — state round-trip
// =============================================================================

TEST(SamplerNeedsWrite, RoundTrip_WriteUpdateSkip)
{
    // Simulates a full write cycle as performed by WriteInstanceSampler:
    //   1. Query SamplerNeedsWrite — must return true on first call (never written).
    //   2. Caller updates its state variables (*inOutGeneration, *inOutID).
    //   3. Query again with same resource — must return false (lazy skip).

    uint32_t inOutGeneration = UINT32_MAX;  // initial sentinel
    uint32_t inOutID         = UINT32_MAX;

    constexpr uint32_t resourceID         = 7u;
    constexpr uint32_t resourceGeneration = 3u;

    // First call — never written, must write.
    EXPECT_TRUE(SamplerNeedsWrite(inOutGeneration, inOutID, resourceID, resourceGeneration));

    // Simulate the mutation WriteInstanceSampler performs after the Vulkan write.
    inOutGeneration = resourceGeneration;
    inOutID         = resourceID;

    // Second call with same resource — already written, skip.
    EXPECT_FALSE(SamplerNeedsWrite(inOutGeneration, inOutID, resourceID, resourceGeneration));
}

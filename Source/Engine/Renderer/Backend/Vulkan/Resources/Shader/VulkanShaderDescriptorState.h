#pragma once

#include "Engine/Renderer/Backend/Vulkan/VulkanConstants.h"

#include <array>
#include <cstdint>

// ── Per-descriptor generation/ID tracking for lazy descriptor writes ──────────
//
// Both arrays default to UINT32_MAX — the "never written" sentinel.
// AcquireInstanceSlot resets them to UINT32_MAX again after pool recreation,
// ensuring the first frame after a hot-reload always forces a fresh write.
struct VulkanShaderDescriptorState
{
    // One slot per swapchain image; only the first swapChainImages.size() are used.
    std::array<uint32_t, MAX_SWAPCHAIN_IMAGES> generations;
    std::array<uint32_t, MAX_SWAPCHAIN_IMAGES> ids;

    // Fill all slots with the "never written" sentinel — a brace initializer can't
    // express MAX_SWAPCHAIN_IMAGES identical values, and zero-filling the tail would
    // wrongly read as "generation 0 / id 0" instead of UINT32_MAX.
    VulkanShaderDescriptorState()
    {
        generations.fill(UINT32_MAX);
        ids.fill(UINT32_MAX);
    }
};

// ── Sampler write-decision logic ──────────────────────────────────────────────
//
// Pure query — no mutation. Call before vkUpdateDescriptorSets; skip the write
// if this returns false. Caller must update *inOutGeneration and *inOutID after
// a successful write.
//
// Returns true when:
//   - inOutGeneration == UINT32_MAX (slot was never written / just reset after hot-reload)
//   - resourceID or resourceGeneration changed since the last write
inline bool SamplerNeedsWrite(uint32_t inOutGeneration, uint32_t inOutID,
                               uint32_t resourceID, uint32_t resourceGeneration)
{
    if (inOutGeneration == UINT32_MAX) return true;
    return !(inOutID == resourceID && inOutGeneration == resourceGeneration);
}

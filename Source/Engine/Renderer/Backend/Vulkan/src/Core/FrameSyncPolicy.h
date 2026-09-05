#pragma once

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>

// -----------------------------------------------------------------------------
// Frame-loop DECISIONS, separated from the Vulkan calls that act on them.
// -----------------------------------------------------------------------------
//
// Every rule here was previously an `if` chain inside a 100+ line method in
// VulkanBackend.cpp, which no test can reach without a device. The rules are the
// part that has actually gone wrong (a wrong SSBO slot made every mesh flicker
// after a resize; treating SUBOPTIMAL like OUT_OF_DATE leaks a signaled
// semaphore) -- so they live here as pure functions and are unit-tested against
// a table, while the calls that execute them stay in the backend.
//
// Everything is constexpr and free of engine types on purpose: the test links
// gtest plus an include dir and nothing else, the same shape as
// VulkanShaderDescriptorState.h next door.

namespace nous::engine::renderer::vulkan
{
    // -------------------------------------------------------------------------
    // Instance-matrix SSBO ring slot
    // -------------------------------------------------------------------------
    //
    // The slot MUST be derived from the swapchain image index, not from a CPU
    // frame counter. It has to match the static binding made once at shader
    // create time -- GlobalSlot(vs, pass, image) -> instanceSSBO[image % kInstanceSSBORingSize]
    // (WriteGlobalStorageDescriptors) -- so that the buffer a draw READS is the one
    // this frame WROTE.
    //
    // Note the ring follows the IMAGE, never the flat global slot. Global set=0
    // resources are allocated per (renderpass, image), so a flat index encodes the
    // pass too and `flatSlot % 3` would silently pick another pass's buffer.
    //
    // Why not a frame counter: RecreateSwapChain resets currentFrame to 0 while a
    // monotonic frame counter keeps advancing, so after any resize the write slot
    // and the read slot drift by a constant offset. Every mesh then reads another
    // frame's matrices and flickers (collapsing to a point -- it looks like the
    // scene emptied). Deriving from imageIndex makes written == read by
    // construction.
    //
    // The modulo is load-bearing, not defensive: the ring is 3 buffers while a
    // swapchain may hand out more images (Mesa llvmpipe reports 4), so image 3
    // must map back onto slot 0 exactly as the binding does.
    inline constexpr uint32_t kInstanceSSBORingSize = 3;

    [[nodiscard]] constexpr uint32_t ChooseInstanceSSBOSlot(const uint32_t imageIndex) noexcept
    {
        return imageIndex % kInstanceSSBORingSize;
    }

    // -------------------------------------------------------------------------
    // Instance-matrix upload clamp
    // -------------------------------------------------------------------------
    //
    // How many of `count` instances may actually be written at `instanceOffset`
    // without running past the end of the SSBO. Returns 0 when the offset is
    // already at or beyond capacity, so a caller that ignores the clamp and
    // writes anyway would corrupt the next pass's matrices rather than merely
    // dropping its own.
    [[nodiscard]] constexpr uint32_t ClampInstanceWriteCount(const uint32_t count,
                                                            const uint32_t instanceOffset,
                                                            const uint32_t capacity) noexcept
    {
        return instanceOffset < capacity ? (std::min)(count, capacity - instanceOffset) : 0u;
    }

    // -------------------------------------------------------------------------
    // Swapchain acquire / present outcomes
    // -------------------------------------------------------------------------

    enum class FrameAction : uint8_t
    {
        Proceed,           // carry on with this frame
        RecreateAndSkip,   // schedule swapchain recreation, drop this frame; not an error
        Fail               // unrecoverable
    };

    // The asymmetry between acquire and present is the whole point of having
    // this as a named function, and it is a genuine Vulkan-spec subtlety rather
    // than a style choice:
    //
    //   OUT_OF_DATE on acquire -> the acquire FAILED. No image was acquired and
    //       imageAvailableSemaphores[currentFrame] was NOT signaled, so skipping
    //       leaves the semaphore clean. Safe to recreate and skip.
    //
    //   SUBOPTIMAL on acquire  -> the acquire SUCCEEDED. An image WAS acquired
    //       and the semaphore IS signaled. Skipping would strand that signaled
    //       semaphore, and the next acquire on the same frame slot would then
    //       signal an already-signaled semaphore (a VUID violation). So we must
    //       render and present this frame normally and let the present path or a
    //       resize drive recreation. Sync objects are not recreated by
    //       RecreateResources, so a stranded signal would persist across resize.
    //
    // `resultIsSuccess` is threaded in rather than recomputed here on purpose.
    // The set of codes Vulkan counts as success is wider than VK_SUCCESS
    // (VK_NOT_READY, VK_TIMEOUT, VK_INCOMPLETE, ...) and it already has exactly
    // one definition in the tree, VkResultIsSuccess in VulkanUtils. Re-deriving
    // it here would put that table in two places that must agree -- the same
    // duplicated-knowledge trap as the built-in shader name matching. The caller
    // passes VkResultIsSuccess(result); this function owns only the acquire/
    // present asymmetry, which is the part with no other home.
    [[nodiscard]] constexpr FrameAction ClassifyAcquireResult(const VkResult result,
                                                              const bool resultIsSuccess) noexcept
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
            return FrameAction::RecreateAndSkip;

        if (result == VK_SUBOPTIMAL_KHR)
            return FrameAction::Proceed;      // semaphore already signaled -- never skip

        return resultIsSuccess ? FrameAction::Proceed : FrameAction::Fail;
    }

    // On present both codes mean the same thing: the frame is already submitted
    // and the semaphore has been consumed by the present's wait, so there is
    // nothing stranded and recreating is safe for either.
    [[nodiscard]] constexpr FrameAction ClassifyPresentResult(const VkResult result,
                                                              const bool resultIsSuccess) noexcept
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            return FrameAction::RecreateAndSkip;

        return resultIsSuccess ? FrameAction::Proceed : FrameAction::Fail;
    }
}

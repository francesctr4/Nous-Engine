// Covers Core/FrameSyncPolicy.h -- the frame-loop decisions extracted out of
// VulkanBackend::BeginFrame / EndFrame / UploadInstanceData.
//
// These rules are the ones that have actually gone wrong in this engine, and
// each of the three had to be diagnosed by running the editor and watching the
// screen, because nothing could reach them from a test:
//
//   * indexing the instance SSBO by a CPU frame counter instead of imageIndex
//     made every mesh read another frame's matrices after a resize (they
//     collapsed to a point -- it looked like the scene had emptied);
//   * treating VK_SUBOPTIMAL_KHR like VK_ERROR_OUT_OF_DATE_KHR on acquire
//     strands a signaled semaphore, and the next acquire on that frame slot then
//     signals an already-signaled semaphore (a VUID violation that passes
//     silently on desktop NVIDIA/AMD);
//   * an unclamped instance write runs past the end of the SSBO into the next
//     pass's matrices.
//
// Links gtest plus one include-dir handle and Vulkan's headers: no engine
// target, no device. Same shape as t_VulkanBackend_VulkanShaderDescriptorState.

#include <gtest/gtest.h>

#include "Core/FrameSyncPolicy.h"

using namespace nous::engine::renderer::vulkan;

// ===========================================================================
// Instance-matrix SSBO ring slot
// ===========================================================================

TEST(t_FrameSyncPolicy, SlotIsTheImageIndexWithinTheRing)
{
    EXPECT_EQ(ChooseInstanceSSBOSlot(0), 0u);
    EXPECT_EQ(ChooseInstanceSSBOSlot(1), 1u);
    EXPECT_EQ(ChooseInstanceSSBOSlot(2), 2u);
}

TEST(t_FrameSyncPolicy, SlotWrapsForASwapchainWiderThanTheRing)
{
    // Mesa llvmpipe reports 4 images while the ring is 3 buffers. minImageCount
    // is a floor a driver may overshoot, so this is a real configuration, not a
    // hypothetical -- image 3 must map back onto slot 0 exactly as the static
    // descriptor binding does.
    EXPECT_EQ(ChooseInstanceSSBOSlot(3), 0u);
    EXPECT_EQ(ChooseInstanceSSBOSlot(4), 1u);
    EXPECT_EQ(ChooseInstanceSSBOSlot(5), 2u);
}

TEST(t_FrameSyncPolicy, SlotIsAlwaysInsideTheRing)
{
    // Up to MAX_SWAPCHAIN_IMAGES (8) and beyond: an out-of-ring slot indexes
    // past instanceSSBOMapped and writes into unmapped memory.
    for (uint32_t imageIndex = 0; imageIndex < 64; ++imageIndex)
        EXPECT_LT(ChooseInstanceSSBOSlot(imageIndex), kInstanceSSBORingSize)
            << "imageIndex = " << imageIndex;
}

TEST(t_FrameSyncPolicy, SlotAgreesWithTheStaticDescriptorBinding)
{
    // The binding made once at shader-create time is
    // GlobalSlot(vs, pass, image) -> instanceSSBO[image % kInstanceSSBORingSize]:
    // the ring follows the IMAGE, not the flat per-(pass, image) global slot.
    // Written out independently here: the upload slot and the binding are two
    // pieces of code that must agree, and this is the assertion that they do.
    for (uint32_t i = 0; i < 16; ++i)
        EXPECT_EQ(ChooseInstanceSSBOSlot(i), i % kInstanceSSBORingSize);
}

TEST(t_FrameSyncPolicy, RingSizeIsThree)
{
    // Pinned deliberately: instanceSSBO / instanceSSBOMapped are std::array<_,3>,
    // so raising this constant alone would index past them.
    EXPECT_EQ(kInstanceSSBORingSize, 3u);
}

// ===========================================================================
// Instance-matrix upload clamp
// ===========================================================================

TEST(t_FrameSyncPolicy, WholeWriteFitsWhenThereIsRoom)
{
    EXPECT_EQ(ClampInstanceWriteCount(10, 0, 100), 10u);
    EXPECT_EQ(ClampInstanceWriteCount(10, 50, 100), 10u);
}

TEST(t_FrameSyncPolicy, WriteIsTruncatedAtCapacity)
{
    EXPECT_EQ(ClampInstanceWriteCount(10, 95, 100), 5u);
    EXPECT_EQ(ClampInstanceWriteCount(1000, 0, 100), 100u);
}

TEST(t_FrameSyncPolicy, ExactFitIsNotTruncated)
{
    EXPECT_EQ(ClampInstanceWriteCount(100, 0, 100), 100u);
    EXPECT_EQ(ClampInstanceWriteCount(1, 99, 100), 1u);
}

TEST(t_FrameSyncPolicy, OffsetAtOrPastCapacityWritesNothing)
{
    // Must be 0, not a wrapped huge number: capacity - instanceOffset underflows
    // on unsigned arithmetic, which would turn an overflow into a colossal memcpy.
    EXPECT_EQ(ClampInstanceWriteCount(10, 100, 100), 0u);
    EXPECT_EQ(ClampInstanceWriteCount(10, 101, 100), 0u);
    EXPECT_EQ(ClampInstanceWriteCount(10, 0xFFFFFFFFu, 100), 0u);
}

TEST(t_FrameSyncPolicy, ZeroCountWritesNothing)
{
    EXPECT_EQ(ClampInstanceWriteCount(0, 0, 100), 0u);
}

TEST(t_FrameSyncPolicy, ZeroCapacityWritesNothing)
{
    EXPECT_EQ(ClampInstanceWriteCount(10, 0, 0), 0u);
}

TEST(t_FrameSyncPolicy, ClampNeverExceedsTheRemainingRoom)
{
    // The property the individual cases are examples of.
    constexpr uint32_t capacity = 64;
    for (uint32_t offset = 0; offset < capacity + 4; ++offset)
    {
        for (const uint32_t count : {0u, 1u, 7u, 64u, 1000u})
        {
            const uint32_t safe = ClampInstanceWriteCount(count, offset, capacity);
            EXPECT_LE(safe, count);
            if (offset < capacity)
                EXPECT_LE(offset + safe, capacity) << "offset=" << offset << " count=" << count;
            else
                EXPECT_EQ(safe, 0u) << "offset=" << offset;
        }
    }
}

// ===========================================================================
// Acquire classification
// ===========================================================================

TEST(t_FrameSyncPolicy, AcquireSuccessProceeds)
{
    EXPECT_EQ(ClassifyAcquireResult(VK_SUCCESS, true), FrameAction::Proceed);
}

TEST(t_FrameSyncPolicy, AcquireOutOfDateRecreatesAndSkips)
{
    // The acquire failed and the semaphore was NOT signaled, so skipping is safe.
    EXPECT_EQ(ClassifyAcquireResult(VK_ERROR_OUT_OF_DATE_KHR, false),
              FrameAction::RecreateAndSkip);
}

TEST(t_FrameSyncPolicy, AcquireSuboptimalProceedsAndDoesNotSkip)
{
    // THE regression guard. Suboptimal means the image WAS acquired and the
    // semaphore IS signaled; returning RecreateAndSkip here strands that signal.
    EXPECT_EQ(ClassifyAcquireResult(VK_SUBOPTIMAL_KHR, true), FrameAction::Proceed);
    EXPECT_NE(ClassifyAcquireResult(VK_SUBOPTIMAL_KHR, true), FrameAction::RecreateAndSkip);
}

TEST(t_FrameSyncPolicy, AcquireSuboptimalProceedsEvenIfNotCountedAsSuccess)
{
    // The suboptimal branch is checked BEFORE the success flag, so a stricter
    // future definition of "success" still cannot turn this into a skip.
    EXPECT_EQ(ClassifyAcquireResult(VK_SUBOPTIMAL_KHR, false), FrameAction::Proceed);
}

TEST(t_FrameSyncPolicy, AcquireHardErrorFails)
{
    EXPECT_EQ(ClassifyAcquireResult(VK_ERROR_DEVICE_LOST, false), FrameAction::Fail);
    EXPECT_EQ(ClassifyAcquireResult(VK_ERROR_OUT_OF_HOST_MEMORY, false), FrameAction::Fail);
    EXPECT_EQ(ClassifyAcquireResult(VK_ERROR_SURFACE_LOST_KHR, false), FrameAction::Fail);
}

TEST(t_FrameSyncPolicy, AcquireNonSuccessCodesTreatedAsSuccessStillProceed)
{
    // VkResultIsSuccess counts VK_NOT_READY / VK_TIMEOUT / VK_INCOMPLETE as
    // success. Preserving that is why the flag is threaded in rather than
    // recomputed as `== VK_SUCCESS`, which would have been a silent behaviour
    // change during the extraction.
    EXPECT_EQ(ClassifyAcquireResult(VK_NOT_READY, true), FrameAction::Proceed);
    EXPECT_EQ(ClassifyAcquireResult(VK_TIMEOUT, true), FrameAction::Proceed);
    EXPECT_EQ(ClassifyAcquireResult(VK_INCOMPLETE, true), FrameAction::Proceed);
}

TEST(t_FrameSyncPolicy, AcquireOutOfDateSkipsRegardlessOfTheSuccessFlag)
{
    // OUT_OF_DATE is checked first, so it cannot be reclassified by the flag.
    EXPECT_EQ(ClassifyAcquireResult(VK_ERROR_OUT_OF_DATE_KHR, true),
              FrameAction::RecreateAndSkip);
}

// ===========================================================================
// Present classification
// ===========================================================================

TEST(t_FrameSyncPolicy, PresentSuccessProceeds)
{
    EXPECT_EQ(ClassifyPresentResult(VK_SUCCESS, true), FrameAction::Proceed);
}

TEST(t_FrameSyncPolicy, PresentOutOfDateRecreatesAndSkips)
{
    EXPECT_EQ(ClassifyPresentResult(VK_ERROR_OUT_OF_DATE_KHR, false),
              FrameAction::RecreateAndSkip);
}

TEST(t_FrameSyncPolicy, PresentSuboptimalRecreatesAndSkips)
{
    // The asymmetry with acquire: by present time the frame is submitted and the
    // semaphore has been consumed by the present's wait, so nothing is stranded
    // and recreating is safe for BOTH codes.
    EXPECT_EQ(ClassifyPresentResult(VK_SUBOPTIMAL_KHR, true),
              FrameAction::RecreateAndSkip);
}

TEST(t_FrameSyncPolicy, PresentHardErrorFails)
{
    EXPECT_EQ(ClassifyPresentResult(VK_ERROR_DEVICE_LOST, false), FrameAction::Fail);
}

TEST(t_FrameSyncPolicy, AcquireAndPresentDisagreeOnSuboptimalAndThatIsTheContract)
{
    // Stated as one assertion so that "simplifying" the two functions into one
    // shared helper fails loudly rather than silently reintroducing the bug.
    EXPECT_EQ(ClassifyAcquireResult(VK_SUBOPTIMAL_KHR, true), FrameAction::Proceed);
    EXPECT_EQ(ClassifyPresentResult(VK_SUBOPTIMAL_KHR, true), FrameAction::RecreateAndSkip);
}

TEST(t_FrameSyncPolicy, AcquireAndPresentAgreeOnOutOfDate)
{
    EXPECT_EQ(ClassifyAcquireResult(VK_ERROR_OUT_OF_DATE_KHR, false),
              ClassifyPresentResult(VK_ERROR_OUT_OF_DATE_KHR, false));
}

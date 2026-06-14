#include "VulkanSyncObjects.h"
#include "Engine/Renderer/Backend/Vulkan/Utils/VulkanUtils.h"

bool NOUS_VulkanSyncObjects::CreateSyncObjects(VulkanContext* vkContext)
{
    bool ret = true;

    const uint32 imageCount = static_cast<uint32>(vkContext->swapChain.swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Per-frame-in-flight: the "image available" semaphore is signaled by vkAcquireNextImageKHR,
    // so it is tied to the CPU frame index (currentFrame), not the swapchain image. The in-flight
    // fences are indexed the same way, so both are sized to maxFramesInFlight.
    vkContext->imageAvailableSemaphores.resize(vkContext->swapChain.maxFramesInFlight);
    vkContext->inFlightFences.resize(vkContext->swapChain.maxFramesInFlight);

    for (uint16 i = 0; i < vkContext->swapChain.maxFramesInFlight; ++i)
    {
        VK_CHECK(vkCreateSemaphore(vkContext->device.logicalDevice, &semaphoreCreateInfo, vkContext->allocator, &vkContext->imageAvailableSemaphores[i]));

        // Create the fence in a signaled state, indicating that the first frame has already been "rendered".
        // This will prevent the application from waiting indefinitely for the first frame to render since it
        // cannot be rendered until a frame is "rendered" before it.
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(vkContext->device.logicalDevice, &fenceCreateInfo, vkContext->allocator, &vkContext->inFlightFences[i]));
    }

    // Per-swapchain-image: the "queue complete" (render-finished) semaphore is waited on by
    // vkQueuePresentKHR, which is tied to the presented image. Indexing it by imageIndex (not
    // currentFrame) prevents re-signaling a binary semaphore whose previous present-wait may
    // still be pending — the image-in-flight fence provides the reuse ordering.
    vkContext->queueCompleteSemaphores.resize(imageCount);

    for (uint32 i = 0; i < imageCount; ++i)
    {
        VK_CHECK(vkCreateSemaphore(vkContext->device.logicalDevice, &semaphoreCreateInfo, vkContext->allocator, &vkContext->queueCompleteSemaphores[i]));
    }

    // imagesInFlight is indexed by swapchain image index, so it is sized to the image count.
    // Entries are non-owning aliases of inFlightFences (set per frame in DrawFrame) and must
    // start at VK_NULL_HANDLE — value-initialization on resize gives exactly that.
    vkContext->imagesInFlight.assign(imageCount, VK_NULL_HANDLE);

    return ret;
}

void NOUS_VulkanSyncObjects::DestroySyncObjects(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Sync Objects...");

    // Per-frame-in-flight: image-available semaphores + in-flight fences.
    for (uint16 i = 0; i < vkContext->swapChain.maxFramesInFlight; ++i)
    {
        if (vkContext->imageAvailableSemaphores[i])
        {
            vkDestroySemaphore(vkContext->device.logicalDevice, vkContext->imageAvailableSemaphores[i], vkContext->allocator);
            vkContext->imageAvailableSemaphores[i] = 0;
        }

        vkDestroyFence(vkContext->device.logicalDevice, vkContext->inFlightFences[i], vkContext->allocator);
    }

    // Per-swapchain-image: queue-complete (render-finished) semaphores.
    for (size_t i = 0; i < vkContext->queueCompleteSemaphores.size(); ++i)
    {
        if (vkContext->queueCompleteSemaphores[i])
        {
            vkDestroySemaphore(vkContext->device.logicalDevice, vkContext->queueCompleteSemaphores[i], vkContext->allocator);
            vkContext->queueCompleteSemaphores[i] = 0;
        }
    }

    vkContext->imageAvailableSemaphores.clear();
    vkContext->imageAvailableSemaphores.shrink_to_fit();

    vkContext->queueCompleteSemaphores.clear();
    vkContext->queueCompleteSemaphores.shrink_to_fit();

    // Fences themselves were destroyed in the loop above; drop the (now non-owning) handles
    // so a subsequent CreateSyncObjects resizes from empty.
    vkContext->inFlightFences.clear();
    vkContext->inFlightFences.shrink_to_fit();

    vkContext->imagesInFlight.clear();
    vkContext->imagesInFlight.shrink_to_fit();
}
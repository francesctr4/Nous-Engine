#include "VulkanSyncObjects.h"
#include "VulkanUtils.h"

bool NOUS_VulkanSyncObjects::CreateSyncObjects(VulkanContext* vkContext)
{
    bool ret = true;

    // Create sync objects.
    vkContext->imageAvailableSemaphores.resize(vkContext->swapChain.maxFramesInFlight);
    vkContext->queueCompleteSemaphores.resize(vkContext->swapChain.maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint16 i = 0; i < vkContext->swapChain.maxFramesInFlight; ++i)
    {
        VK_CHECK(vkCreateSemaphore(vkContext->device.logicalDevice, &semaphoreCreateInfo, vkContext->allocator, &vkContext->imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(vkContext->device.logicalDevice, &semaphoreCreateInfo, vkContext->allocator, &vkContext->queueCompleteSemaphores[i]));

        // Create the fence in a signaled state, indicating that the first frame has already been "rendered".
        // This will prevent the application from waiting indefinitely for the first frame to render since it
        // cannot be rendered until a frame is "rendered" before it.
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(vkContext->device.logicalDevice, &fenceCreateInfo, vkContext->allocator, &vkContext->inFlightFences[i]));
    }

    // In flight fences should not yet exist at this point, so clear the list. These are stored in pointers
    // because the initial state should be 0, and will be 0 when not in use. Acutal fences are not owned
    // by this list.

    for (uint16 i = 0; i < vkContext->imagesInFlight.size(); ++i)
    {
        vkContext->imagesInFlight[i] = 0;
    }

    return ret;
}

void NOUS_VulkanSyncObjects::DestroySyncObjects(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Sync Objects...");

    for (uint16 i = 0; i < vkContext->swapChain.maxFramesInFlight; ++i) 
    {
        if (vkContext->imageAvailableSemaphores[i]) 
        {
            vkDestroySemaphore(vkContext->device.logicalDevice, vkContext->imageAvailableSemaphores[i], vkContext->allocator);
            vkContext->imageAvailableSemaphores[i] = 0;
        }

        if (vkContext->queueCompleteSemaphores[i])
        {
            vkDestroySemaphore(vkContext->device.logicalDevice, vkContext->queueCompleteSemaphores[i], vkContext->allocator);
            vkContext->queueCompleteSemaphores[i] = 0;
        }

        vkDestroyFence(vkContext->device.logicalDevice, vkContext->inFlightFences[i], vkContext->allocator);
    }

    vkContext->imageAvailableSemaphores.clear();
    vkContext->imageAvailableSemaphores.shrink_to_fit();

    vkContext->queueCompleteSemaphores.clear();
    vkContext->queueCompleteSemaphores.shrink_to_fit();
}
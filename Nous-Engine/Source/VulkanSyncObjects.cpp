#include "VulkanSyncObjects.h"

bool NOUS_VulkanSyncObjects::CreateSyncObjects(VulkanContext* vkContext)
{
    bool ret = true;

    // Create sync objects.
    vkContext->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    vkContext->queueCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    vkContext->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint16 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkCreateSemaphore(vkContext->device.logicalDevice, &semaphoreCreateInfo, vkContext->allocator, &vkContext->imageAvailableSemaphores[i]);
        vkCreateSemaphore(vkContext->device.logicalDevice, &semaphoreCreateInfo, vkContext->allocator, &vkContext->queueCompleteSemaphores[i]);

        // Create the fence in a signaled state, indicating that the first frame has already been "rendered".
        // This will prevent the application from waiting indefinitely for the first frame to render since it
        // cannot be rendered until a frame is "rendered" before it.
        CreateVulkanFence(vkContext, true, &vkContext->inFlightFences[i]);
    }

    // In flight fences should not yet exist at this point, so clear the list. These are stored in pointers
    // because the initial state should be 0, and will be 0 when not in use. Acutal fences are not owned
    // by this list.

    vkContext->imagesInFlight.resize(vkContext->swapChain.swapChainImages.size());

    for (uint16 i = 0; i < vkContext->imagesInFlight.size(); ++i)
    {
        vkContext->imagesInFlight[i] = 0;
    }

    return ret;
}

void NOUS_VulkanSyncObjects::DestroySyncObjects(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Sync Objects...");

    for (uint16 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
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

        DestroyVulkanFence(vkContext, &vkContext->inFlightFences[i]);
    }

    vkContext->imageAvailableSemaphores.clear();
    vkContext->imageAvailableSemaphores.shrink_to_fit();

    vkContext->queueCompleteSemaphores.clear();
    vkContext->queueCompleteSemaphores.shrink_to_fit();

    vkContext->inFlightFences.clear();
    vkContext->inFlightFences.shrink_to_fit();

    vkContext->imagesInFlight.clear();
    vkContext->imagesInFlight.shrink_to_fit();
}

void NOUS_VulkanSyncObjects::CreateVulkanFence(VulkanContext* vkContext, bool createSignaled, VulkanFence* outFence)
{
    // Make sure to signal the fence if required.
    outFence->isSignaled = createSignaled;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = (outFence->isSignaled) ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VK_CHECK(vkCreateFence(vkContext->device.logicalDevice, &fenceCreateInfo, vkContext->allocator, &outFence->handle));
}

void NOUS_VulkanSyncObjects::DestroyVulkanFence(VulkanContext* vkContext, VulkanFence* fence)
{
    if (fence->handle) 
    {
        vkDestroyFence(vkContext->device.logicalDevice, fence->handle, vkContext->allocator);
        fence->handle = 0;
    }

    fence->isSignaled = false;
}

bool NOUS_VulkanSyncObjects::WaitOnVulkanFence(VulkanContext* vkContext, VulkanFence* fence, uint64 timeout_ns)
{
    if (!fence->isSignaled) 
    {
        VkResult result = vkWaitForFences(vkContext->device.logicalDevice, 1, &fence->handle, true, timeout_ns);

        switch (result) 
        {
            case VK_SUCCESS: 
            {
                fence->isSignaled = true;
                return true;
            }
            case VK_TIMEOUT: 
            {
                NOUS_WARN("vkWaitForFences - Timed out");
                break;
            }      
            case VK_ERROR_DEVICE_LOST: 
            {
                NOUS_ERROR("vkWaitForFences - VK_ERROR_DEVICE_LOST.");
                break;
            }
            case VK_ERROR_OUT_OF_HOST_MEMORY: 
            {
                NOUS_ERROR("vkWaitForFences - VK_ERROR_OUT_OF_HOST_MEMORY.");
                break;
            }
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: 
            {
                NOUS_ERROR("vkWaitForFences - VK_ERROR_OUT_OF_DEVICE_MEMORY.");
                break;
            }
            default: 
            {
                NOUS_ERROR("vkWaitForFences - An unknown error has occurred.");
                break;
            }
            
        }
    }
    else 
    {
        // If already signaled, do not wait.
        return true;
    }

    return false;
}

void NOUS_VulkanSyncObjects::ResetVulkanFence(VulkanContext* vkContext, VulkanFence* fence)
{
    if (fence->isSignaled) 
    {
        VK_CHECK(vkResetFences(vkContext->device.logicalDevice, 1, &fence->handle));
        fence->isSignaled = false;
    }
}

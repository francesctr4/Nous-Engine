#include "VulkanSwapchain.h"
#include "VulkanPlatform.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"

#include "Logger.h"
#include "MemoryManager.h"

bool CreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain)
{
    bool ret = true;

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(vkContext->device.swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(vkContext->device.swapChainSupport.presentModes);

    vkContext->device.swapChainSupport = QuerySwapChainSupport(vkContext->device.physicalDevice, vkContext);

    VkExtent2D extent = ChooseSwapExtent(vkContext->device.swapChainSupport.capabilities);

    uint32_t imageCount = vkContext->device.swapChainSupport.capabilities.minImageCount + 1;

    if (vkContext->device.swapChainSupport.capabilities.maxImageCount > 0 && imageCount > vkContext->device.swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = vkContext->device.swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vkContext->surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { vkContext->device.graphicsQueueIndex, vkContext->device.presentQueueIndex };

    if (vkContext->device.graphicsQueueIndex != vkContext->device.presentQueueIndex) 
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else 
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = vkContext->device.swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK_MSG(vkCreateSwapchainKHR(vkContext->device.logicalDevice, &createInfo, vkContext->allocator, &swapChain->handle), "vkCreateSwapchainKHR failed!");

    vkContext->currentFrame = 0;

    VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device.logicalDevice, swapChain->handle, &imageCount, nullptr));
    vkContext->swapChain.swapChainImages.resize(imageCount);

    VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device.logicalDevice, swapChain->handle, &imageCount, swapChain->swapChainImages.data()));

    swapChain->swapChainImageFormat = surfaceFormat.format;
    swapChain->swapChainExtent = extent;

    swapChain->swapChainImageViews.resize(swapChain->swapChainImages.size());

    for (uint32_t i = 0; i < swapChain->swapChainImages.size(); ++i)
    {
        // Temporary VulkanImage object to hold the created view.
        VulkanImage tempImage;
        tempImage.handle = vkContext->swapChain.swapChainImages[i];

        // Call CreateVulkanImageView to populate tempImage.view.
        CreateVulkanImageView(vkContext, vkContext->swapChain.swapChainImageFormat, &tempImage, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        // Assign the created view to the vector.
        swapChain->swapChainImageViews[i] = tempImage.view;
    }

    // Depth resources
    vkContext->device.depthFormat = FindDepthFormat(vkContext->device.physicalDevice);

    // Create depth image and its view.
    CreateVulkanImage(
        vkContext,
        VK_IMAGE_TYPE_2D,
        extent.width,
        extent.height,
        1,
        vkContext->device.msaaSamples,
        vkContext->device.depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        &swapChain->depthAttachment);

    return ret;
}

void RecreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain)
{
    DestroySwapChain(vkContext, swapChain);
    CreateSwapChain(vkContext, width, height, swapChain);
}

void DestroySwapChain(VulkanContext* vkContext, VulkanSwapChain* swapChain)
{
    DestroyVulkanImage(vkContext, &swapChain->depthAttachment);

    // Only destroy the views, not the images, since those are owned by the swapchain and are thus destroyed when it is.
    for (VkImageView imageView : swapChain->swapChainImageViews) 
    {
        vkDestroyImageView(vkContext->device.logicalDevice, imageView, vkContext->allocator);
    }

    vkDestroySwapchainKHR(vkContext->device.logicalDevice, swapChain->handle, vkContext->allocator);
}

bool SwapChainAcquireNextImageIndex(VulkanContext* vkContext, VulkanSwapChain* swapchain, uint64 timeout_ns,
    VkSemaphore imageAvailableSemaphore, VkFence fence, uint32* outImageIndex)
{
    VkResult result = vkAcquireNextImageKHR(vkContext->device.logicalDevice, swapchain->handle, timeout_ns,
        imageAvailableSemaphore, fence, outImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        // Trigger swapchain recreation, then boot out of the render loop.
        RecreateSwapChain(vkContext, vkContext->framebufferWidth, vkContext->framebufferHeight, swapchain);
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) 
    {
        NOUS_FATAL("Failed to acquire swapchain image!");
        return false;
    }

    return true;
}

void SwapChainPresent(VulkanContext* vkContext, VulkanSwapChain* swapchain, VkQueue graphicsQueue,
    VkQueue presentQueue, VkSemaphore renderCompleteSemaphore, uint32 presentImageIndex)
{
    // Return the image to the swapchain for presentation.
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain->handle;

    presentInfo.pImageIndices = &presentImageIndex;
    presentInfo.pResults = 0;

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) 
    {
        // Swapchain is out of date, suboptimal or a framebuffer resize has occurred. Trigger swapchain recreation.
        RecreateSwapChain(vkContext, vkContext->framebufferWidth, vkContext->framebufferHeight, swapchain);
    }
    else if (result != VK_SUCCESS) 
    {
        NOUS_FATAL("Failed to present swap chain image!");
    }
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {

        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            return availableFormat;

        }

    }

    return availableFormats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {

        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {

            return availablePresentMode; // Triple Buffering

        }

    }

    return VK_PRESENT_MODE_FIFO_KHR; // Vertical Sync
}

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {

        return capabilities.currentExtent;

    }
    else {

        int width, height;
        SDL_Vulkan_GetDrawableSize(GetSDLWindowData(), &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}
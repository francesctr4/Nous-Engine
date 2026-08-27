#include "VulkanSwapchain.h"
#include "Core/Device/VulkanDevice.h"
#include "Resources/Image/VulkanImage.h"
#include "Utils/VulkanUtils.h"
#include "VulkanConstants.h"

#include <Logger/Logger.h>
#include <Renderer/iRenderWindow.h>
#include <algorithm>  // Required for std::clamp

bool NOUS_VulkanSwapChain::CreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain)
{
    bool ret = true;

    // Re-query support up front so format, present mode AND extent are all chosen from the
    // same fresh snapshot. (On recreation the cached snapshot from PickPhysicalDevice can be
    // stale after display/output changes.)
    vkContext->device.swapChainSupport = NOUS_VulkanDevice::QuerySwapChainSupport(vkContext->device.physicalDevice, vkContext);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(vkContext->device.swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(vkContext->device.swapChainSupport.presentModes);

    VkExtent2D extent = ChooseSwapExtent(vkContext, vkContext->device.swapChainSupport.capabilities);

    uint32_t imageCount = vkContext->device.swapChainSupport.capabilities.minImageCount + 1;

    if (vkContext->device.swapChainSupport.capabilities.maxImageCount > 0 && imageCount > vkContext->device.swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = vkContext->device.swapChainSupport.capabilities.maxImageCount;
    }

    swapChain->maxFramesInFlight = imageCount - 1;

    VkSwapchainCreateInfoKHR createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vkContext->surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32 queueFamilyIndices[] = { static_cast<uint32>(vkContext->device.graphicsQueueIndex), static_cast<uint32>(vkContext->device.presentQueueIndex) };

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

    // The driver may return more images than requested (minImageCount is a floor — e.g. Mesa
    // llvmpipe returns 4). Every per-swapchain-image inline array is capped at MAX_SWAPCHAIN_IMAGES,
    // so fail loud (and, in release, fail init cleanly) rather than overrun if a driver exceeds it.
    NOUS_ASSERT_MSG(imageCount <= MAX_SWAPCHAIN_IMAGES,
        "Swapchain image count exceeds MAX_SWAPCHAIN_IMAGES — raise the cap.");
    if (imageCount > MAX_SWAPCHAIN_IMAGES)
    {
        NOUS_ERROR("[VulkanSwapchain] Swapchain reports %u images, exceeding MAX_SWAPCHAIN_IMAGES (%u).",
            imageCount, MAX_SWAPCHAIN_IMAGES);
        return false;
    }

    vkContext->swapChain.swapChainImages.resize(imageCount);

    VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device.logicalDevice, swapChain->handle, &imageCount, swapChain->swapChainImages.data()));

    swapChain->swapChainImageFormat = surfaceFormat.format;
    swapChain->swapChainExtent = extent;

    // The surface's currentExtent can differ from the requested window size (e.g. X11/llvmpipe
    // clamps it by a few px). Make framebufferWidth/Height authoritative = the real swapchain
    // extent so framebuffers and offscreen images are sized to match the swapchain images
    // exactly — otherwise vkCreateFramebuffer fails VUID-VkFramebufferCreateInfo-flags-04534.
    vkContext->framebufferWidth  = static_cast<int32>(extent.width);
    vkContext->framebufferHeight = static_cast<int32>(extent.height);

    swapChain->swapChainImageViews.resize(swapChain->swapChainImages.size());

    for (uint32_t i = 0; i < swapChain->swapChainImages.size(); ++i)
    {
        // Temporary VulkanImage object to hold the created view.
        VulkanImage tempImage;
        tempImage.handle = vkContext->swapChain.swapChainImages[i];

        // Call CreateVulkanImageView to populate tempImage.view.
        NOUS_VulkanImage::CreateVulkanImageView(vkContext, vkContext->swapChain.swapChainImageFormat, &tempImage, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        // Assign the created view to the vector.
        swapChain->swapChainImageViews[i] = tempImage.view;
    }

    CreateColorResources(vkContext, swapChain);

    CreateDepthResources(vkContext, swapChain);

    return ret;
}

void NOUS_VulkanSwapChain::RecreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain)
{
    DestroySwapChain(vkContext, swapChain);
    CreateSwapChain(vkContext, width, height, swapChain);
}

void NOUS_VulkanSwapChain::DestroySwapChain(VulkanContext* vkContext, VulkanSwapChain* swapChain)
{
    NOUS_DEBUG("Destroying Swap Chain...");

    NOUS_VulkanImage::DestroyVulkanImage(vkContext, &swapChain->colorAttachment);
    NOUS_VulkanImage::DestroyVulkanImage(vkContext, &swapChain->depthAttachment);

    // Only destroy the views, not the images, since those are owned by the swapchain and are thus destroyed when it is.
    for (VkImageView imageView : swapChain->swapChainImageViews) 
    {
        vkDestroyImageView(vkContext->device.logicalDevice, imageView, vkContext->allocator);
    }

    vkDestroySwapchainKHR(vkContext->device.logicalDevice, swapChain->handle, vkContext->allocator);
}

VkResult NOUS_VulkanSwapChain::SwapChainAcquireNextImageIndex(VulkanContext* vkContext, VulkanSwapChain* swapchain, uint64 timeout_ns,
    VkSemaphore imageAvailableSemaphore, VkFence fence, uint32* outImageIndex)
{
    VkResult result = vkAcquireNextImageKHR(vkContext->device.logicalDevice, swapchain->handle, timeout_ns,
        imageAvailableSemaphore, fence, outImageIndex);

    // OUT_OF_DATE and SUBOPTIMAL are returned to the caller (VulkanBackend::BeginFrame)
    // which handles recreation lazily via RecreateResources().  Recreating eagerly here
    // would cause a double recreation — once now and once in RecreateResources().
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        NOUS_FATAL("Failed to acquire swapchain image!");
    }

    return result;
}

VkResult NOUS_VulkanSwapChain::SwapChainPresent(VulkanContext* vkContext, VulkanSwapChain* swapchain, VkQueue graphicsQueue,
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
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    // OUT_OF_DATE and SUBOPTIMAL are returned to the caller (VulkanBackend::EndFrame)
    // which schedules lazy recreation via RecreateResources().
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        NOUS_FATAL("Failed to present swap chain image!");
    }

    // Increment (and loop) the index.
    vkContext->currentFrame = (vkContext->currentFrame + 1) % swapchain->maxFramesInFlight;

    return result;
}

VkSurfaceFormatKHR NOUS_VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {

        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            return availableFormat;

        }

    }

    return availableFormats[0];
}

VkPresentModeKHR NOUS_VulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {

        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {

            return availablePresentMode; // Triple Buffering

        }

    }

    return VK_PRESENT_MODE_FIFO_KHR; // Vertical Sync
}

VkExtent2D NOUS_VulkanSwapChain::ChooseSwapExtent(VulkanContext* vkContext, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int32 width, height;
        vkContext->window->GetFramebufferSize(&width, &height);

        VkExtent2D actualExtent = { static_cast<uint32>(width), static_cast<uint32>(height) };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void NOUS_VulkanSwapChain::CreateColorResources(VulkanContext* vkContext, VulkanSwapChain* swapchain)
{
    // Color resources
    vkContext->device.colorFormat = swapchain->swapChainImageFormat;

    // Create color image and its view.
    NOUS_VulkanImage::CreateVulkanImage(
        vkContext,
        VK_IMAGE_TYPE_2D,
        swapchain->swapChainExtent.width,
        swapchain->swapChainExtent.height,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        vkContext->device.colorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_COLOR_BIT,
        &swapchain->colorAttachment);
}

void NOUS_VulkanSwapChain::CreateDepthResources(VulkanContext* vkContext, VulkanSwapChain* swapchain)
{
    // Depth resources
    vkContext->device.depthFormat = NOUS_VulkanDevice::FindDepthFormat(vkContext->device.physicalDevice);

    // Create depth image and its view.
    NOUS_VulkanImage::CreateVulkanImage(
        vkContext,
        VK_IMAGE_TYPE_2D,
        swapchain->swapChainExtent.width,
        swapchain->swapChainExtent.height,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        vkContext->device.depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        &swapchain->depthAttachment);
}

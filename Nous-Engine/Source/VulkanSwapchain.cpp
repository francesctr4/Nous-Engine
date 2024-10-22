#include "VulkanSwapchain.h"
#include "VulkanPlatform.h"

bool CreateSwapChain(VulkanContext* vkContext)
{
    bool ret = true;

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(vkContext->device.swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(vkContext->device.swapChainSupport.presentModes);
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

    VK_CHECK_MSG(vkCreateSwapchainKHR(vkContext->device.logicalDevice, &createInfo, vkContext->allocator, &vkContext->swapChain.swapChainHandle), "vkCreateSwapchainKHR failed!");

    VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device.logicalDevice, vkContext->swapChain.swapChainHandle, &imageCount, nullptr));
    vkContext->swapChain.swapChainImages.resize(imageCount);

    VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device.logicalDevice, vkContext->swapChain.swapChainHandle, &imageCount, vkContext->swapChain.swapChainImages.data()));

    vkContext->swapChain.swapChainImageFormat = surfaceFormat.format;
    vkContext->swapChain.swapChainExtent = extent;

    return ret;
}

void RecreateSwapChain(VulkanContext* vkContext)
{

}

void DestroySwapChain(VulkanContext* vkContext)
{

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
#ifndef VULKANSWAPCHAIN_H
#define VULKANSWAPCHAIN_H

#include "Engine/Core/Globals.h"
#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"

namespace NOUS_VulkanSwapChain
{
    bool CreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain);
    void RecreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain);
    void DestroySwapChain(VulkanContext* vkContext, VulkanSwapChain* swapChain);

    VkResult SwapChainAcquireNextImageIndex(
        VulkanContext* vkContext,
        VulkanSwapChain* swapchain,
        uint64 timeout_ns,
        VkSemaphore imageAvailableSemaphore,
        VkFence fence,
        uint32* outImageIndex);

    VkResult SwapChainPresent(
        VulkanContext* vkContext,
        VulkanSwapChain* swapchain,
        VkQueue graphicsQueue,
        VkQueue presentQueue,
        VkSemaphore renderCompleteSemaphore,
        uint32 presentImageIndex);

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void CreateColorResources(VulkanContext* vkContext, VulkanSwapChain* swapchain);

    void CreateDepthResources(VulkanContext* vkContext, VulkanSwapChain* swapchain);
}

#endif // VULKANSWAPCHAIN_H
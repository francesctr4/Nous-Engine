#ifndef VULKANSWAPCHAIN_H
#define VULKANSWAPCHAIN_H

#include "VulkanTypes.inl"
#include <cstdint>

namespace NOUS_VulkanSwapChain
{
    bool CreateSwapChain(VulkanContext* vkContext, uint32_t width, uint32_t height, VulkanSwapChain* swapChain);
    void RecreateSwapChain(VulkanContext* vkContext, uint32_t width, uint32_t height, VulkanSwapChain* swapChain);
    void DestroySwapChain(VulkanContext* vkContext, VulkanSwapChain* swapChain);

    VkResult SwapChainAcquireNextImageIndex(
        VulkanContext* vkContext,
        VulkanSwapChain* swapchain,
        uint64_t timeout_ns,
        VkSemaphore imageAvailableSemaphore,
        VkFence fence,
        uint32_t* outImageIndex);

    VkResult SwapChainPresent(
        VulkanContext* vkContext,
        VulkanSwapChain* swapchain,
        VkQueue graphicsQueue,
        VkQueue presentQueue,
        VkSemaphore renderCompleteSemaphore,
        uint32_t presentImageIndex);

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(VulkanContext* vkContext, const VkSurfaceCapabilitiesKHR& capabilities);

    void CreateColorResources(VulkanContext* vkContext, VulkanSwapChain* swapchain);

    void CreateDepthResources(VulkanContext* vkContext, VulkanSwapChain* swapchain);
}

#endif // VULKANSWAPCHAIN_H
#pragma once

#include "Globals.h"
#include "VulkanTypes.inl"

bool CreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain);
void RecreateSwapChain(VulkanContext* vkContext, uint32 width, uint32 height, VulkanSwapChain* swapChain);
void DestroySwapChain(VulkanContext* vkContext, VulkanSwapChain* swapChain);

bool SwapChainAcquireNextImageIndex(
    VulkanContext* vkContext,
    VulkanSwapChain* swapchain,
    uint64 timeout_ns,
    VkSemaphore imageAvailableSemaphore,
    VkFence fence,
    uint32* outImageIndex);

void SwapChainPresent(
    VulkanContext* vkContext,
    VulkanSwapChain* swapchain,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    VkSemaphore renderCompleteSemaphore,
    uint32 presentImageIndex);

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
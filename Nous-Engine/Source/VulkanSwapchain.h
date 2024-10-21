#pragma once

#include "Globals.h"
#include "VulkanTypes.inl"

bool CreateSwapChain(VulkanContext* vkContext);
//void RecreateSwapChain();
void DestroySwapChain(VulkanContext* vkContext);

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
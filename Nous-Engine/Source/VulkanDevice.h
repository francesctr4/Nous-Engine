#pragma once

#include "Globals.h"
#include "VulkanTypes.inl"

#include "DynamicArray.h"

const std::array<std::string, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

struct VkQueueFamilyIndices
{
	std::optional<uint32> graphicsFamily;
	std::optional<uint32> presentFamily;

	bool IsComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VulkanContext* vkContext);

VkQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VulkanContext* vkContext);

bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, VulkanContext* vkContext);

VkSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VulkanContext* vkContext);

void LogInfoAboutDevice(VulkanContext* vkContext);
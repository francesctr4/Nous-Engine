#pragma once

#include "Globals.h"
#include "VulkanTypes.inl"

const std::array<std::string, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

struct VkPhysicalDeviceQueueFamilyIndices
{
	std::optional<uint32> graphicsFamilyIndex;
	std::optional<uint32> presentFamilyIndex;
	std::optional<uint32> computeFamilyIndex;
	std::optional<uint32> transferFamilyIndex;

	bool IsComplete()
	{
		return graphicsFamilyIndex.has_value() && presentFamilyIndex.has_value() &&
			computeFamilyIndex.has_value() && transferFamilyIndex.has_value();
	}
};

struct VkPhysicalDeviceRequirements
{
	bool discreteGPU;
	bool geometryShader;
	bool samplerAnisotropy;
	bool queueFamilies;
	bool extensionsSupported;
	bool swapChainAdequate;
};

bool IsPhysicalDeviceSuitable(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

VkPhysicalDeviceQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

bool CheckDeviceExtensionSupport(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

VkSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

// --------------- Multisampling --------------- //
VkSampleCountFlagBits GetMaxUsableSampleCount(const VkPhysicalDeviceProperties& properties);

void LogInfoAboutDevice(VulkanContext* vkContext);
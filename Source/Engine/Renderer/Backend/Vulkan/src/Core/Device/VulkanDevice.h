#ifndef VULKANDEVICE_H
#define VULKANDEVICE_H

#include "Engine/Core/Globals.h"
#include "VulkanTypes.inl"

#include <optional>

#ifdef __APPLE__
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset" };
#else
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#endif

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

	bool Completed() const
	{
		// NOTE: discreteGPU is NOT a hard requirement — it is a *preference* applied
		// in PickPhysicalDevice (discrete is chosen first, integrated/software is a
		// fallback). This lets the engine run on integrated GPUs and on software
		// rasterizers like llvmpipe (e.g. a headless/VM environment with no real GPU).
#ifdef __APPLE__
		// MoltenVK/Apple Silicon: no geometry shader support either.
		return samplerAnisotropy && queueFamilies && extensionsSupported && swapChainAdequate;
#else
		return geometryShader && samplerAnisotropy &&
			queueFamilies && extensionsSupported && swapChainAdequate;
#endif
	}
};

namespace NOUS_VulkanDevice 
{
	// ----------------------------------------------------------- //
	// --------------------- Physical Device --------------------- //
	// ----------------------------------------------------------- //

	bool PickPhysicalDevice(VulkanContext* vkContext);

	bool IsPhysicalDeviceSuitable(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

	VkPhysicalDeviceQueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& physicalDevice, const VulkanContext* vkContext);

	bool CheckDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

	VkSwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& physicalDevice, const VulkanContext* vkContext);

	int32 FindMemoryIndex(const VkPhysicalDevice& physicalDevice, uint32 typeFilter, VkMemoryPropertyFlags properties);

	VkFormat FindDepthFormat(const VkPhysicalDevice& physicalDevice);
	VkFormat FindSupportedFormat(const VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	VkSampleCountFlagBits GetMaxUsableSampleCount(const VkPhysicalDeviceProperties& properties); // Multisampling

	/**
	 * @brief Logs detailed information about the selected Vulkan physical device.
	 * @param vkContext: The Vulkan context containing information about the selected device.
	 */
	void LogInfoAboutDevice(VulkanContext* vkContext);

	// ----------------------------------------------------------- //
	// ---------------------- Logical Device --------------------- //
	// ----------------------------------------------------------- //

	bool CreateLogicalDevice(VulkanContext* vkContext);

	void CreateCommandPool(VulkanContext* vkContext);

	void DestroyLogicalDevice(VulkanContext* vkContext);
}

#endif // VULKANDEVICE_H
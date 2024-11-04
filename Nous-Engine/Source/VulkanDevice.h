/********************************************************************************************
 *																							*
 *  Project Name: [ Nous Engine: a multithreaded game engine built with Vulkan in C++ ]		*
 *  File Name:    [ VulkanDevice.h ]														*
 *  Description:  [ Header file for Managing Vulkan Physical and Logical Device ]			*
 *																							*
 *  Author:       [ Francesc Teruel Rodríguez ]												*
 *  GitHub:       [ https://github.com/francesctr4 ]										*
 *																							*
 ********************************************************************************************/

#pragma once

#include "Globals.h"
#include "VulkanTypes.inl"

const std::array<const char*, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

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

	bool Completed() 
	{
		return discreteGPU && geometryShader && samplerAnisotropy &&
			queueFamilies && extensionsSupported && swapChainAdequate;
	}
};

// ----------------------------------------------------------- //
// --------------------- Physical Device --------------------- //
// ----------------------------------------------------------- //

bool PickPhysicalDevice(VulkanContext* vkContext);

bool IsPhysicalDeviceSuitable(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

VkPhysicalDeviceQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

bool CheckDeviceExtensionSupport(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

VkSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext);

int32 FindMemoryType(VkPhysicalDevice& physicalDevice, uint32 typeFilter, VkMemoryPropertyFlags properties);

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

void DestroyLogicalDevice(VulkanContext* vkContext);
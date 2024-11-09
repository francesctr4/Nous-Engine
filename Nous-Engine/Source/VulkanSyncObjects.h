#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanSyncObjects 
{
	bool CreateSyncObjects(VulkanContext* vkContext);
	void DestroySyncObjects(VulkanContext* vkContext);

	// ------------------------------------------------------------------------------------------------ //
	
	// Vulkan Fence

	void CreateVulkanFence(VulkanContext* vkContext, bool createSignaled, VulkanFence* outFence);

	void DestroyVulkanFence(VulkanContext* vkContext, VulkanFence* fence);

	bool WaitOnVulkanFence(VulkanContext* vkContext, VulkanFence* fence, uint64 timeout_ns);

	void ResetVulkanFence(VulkanContext* vkContext, VulkanFence* fence);
}
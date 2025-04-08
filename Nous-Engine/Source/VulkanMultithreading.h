#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanMultithreading 
{
	bool CreateWorkerCommandPools(VulkanContext* vkContext);
	bool RecreateWorkerCommandPools(VulkanContext* vkContext);
	bool DestroyWorkerCommandPools(VulkanContext* vkContext);

    VkCommandPool GetThreadCommandPool(VulkanContext* vkContext, uint32 threadID);
}
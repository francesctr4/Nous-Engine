#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanMultithreading 
{
	bool CreateWorkerCommandPools(VulkanContext* vkContext);
	bool RecreateWorkerCommandPools(VulkanContext* vkContext);
	bool DestroyWorkerCommandPools(VulkanContext* vkContext);

    VkCommandPool GetThreadCommandPool(VulkanContext* vkContext, uint32 threadID);

	bool QueueSubmitThreadSafe(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle);
	bool CreateQueueSubmitTask(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle);
}
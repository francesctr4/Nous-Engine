#ifndef VULKANMULTITHREADING_H
#define VULKANMULTITHREADING_H

#include "VulkanTypes.inl"
namespace NOUS_VulkanMultithreading 
{
	bool CreateWorkerCommandPools(VulkanContext* vkContext);
	bool RecreateWorkerCommandPools(VulkanContext* vkContext);
	bool DestroyWorkerCommandPools(VulkanContext* vkContext);

    VkCommandPool GetThreadCommandPool(VulkanContext* vkContext, std::thread::id threadID);

	bool CreateQueueSubmitTask(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle);
}

#endif // VULKANMULTITHREADING_H
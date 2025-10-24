#ifndef VULKANMULTITHREADING_H
#define VULKANMULTITHREADING_H

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/EngineExport.h"

namespace NOUS_VulkanMultithreading 
{
	bool CreateWorkerCommandPools(VulkanContext* vkContext);
	NOUS_ENGINE_API bool RecreateWorkerCommandPools(VulkanContext* vkContext);
	bool DestroyWorkerCommandPools(VulkanContext* vkContext);

    VkCommandPool GetThreadCommandPool(VulkanContext* vkContext, uint32 threadID);

	bool QueueSubmitThreadSafe(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle);
	bool CreateQueueSubmitTask(VulkanContext* vkContext, VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence, bool waitIdle);
}

#endif // VULKANMULTITHREADING_H
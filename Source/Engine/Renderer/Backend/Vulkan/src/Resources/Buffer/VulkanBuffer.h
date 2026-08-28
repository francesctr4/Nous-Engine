#ifndef VULKANBUFFER_H
#define VULKANBUFFER_H

#include "VulkanTypes.inl"

namespace NOUS_VulkanBuffer
{
	bool CreateBuffers(VulkanContext* vkContext);
	void DestroyBuffers(VulkanContext* vkContext);

	// -------------------------------------------------------------------------------------------------------- //

	bool CreateBuffer(VulkanContext* vkContext, uint64_t size, VkBufferUsageFlagBits usage,
		uint32_t memoryPropertyFlags, bool bindOnCreate, VulkanBuffer* outBuffer);

	void DestroyBuffer(VulkanContext* vkContext, VulkanBuffer* buffer);

	bool ResizeBuffer(VulkanContext* vkContext, uint64_t newSize, 
		VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool);

	void CopyBuffer(VulkanContext* vkContext, VkCommandPool pool, VkFence fence, VkQueue queue,
		VkBuffer source, uint64_t sourceOffset, VkBuffer dest, uint64_t destOffset, uint64_t size);
	
	void LoadData(VulkanContext* vkContext, VulkanBuffer* buffer,
		uint64_t offset, uint64_t size, uint32_t flags, const void* data);

	void BindBuffer(VulkanContext* vkContext, VulkanBuffer* buffer, VkDeviceSize memoryOffset);

	void* LockMemory(VulkanContext* vkContext, VulkanBuffer* buffer, uint64_t offset, uint64_t size, uint32_t flags);
	void UnlockMemory(VulkanContext* vkContext, VulkanBuffer* buffer);

	bool Allocate(VulkanBuffer* buffer, uint64_t size, uint64_t* outOffset);
	bool Free(VulkanBuffer* buffer, uint64_t size, uint64_t offset);

	// -------------------------------------------------------------------------------------------------------- //

	bool UploadDataRange(VulkanContext* vkContext, VkCommandPool pool, VkFence fence, VkQueue queue,
		VulkanBuffer* buffer, uint64_t* outOffset, uint64_t size, const void* data);

	void FreeDataRange(VulkanContext* vkContext, VulkanBuffer* buffer, uint64_t offset, uint64_t size);
}

#endif // VULKANBUFFER_H
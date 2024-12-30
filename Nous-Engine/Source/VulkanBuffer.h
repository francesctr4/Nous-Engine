#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanBuffer
{
	bool CreateBuffers(VulkanContext* vkContext);
	void DestroyBuffers(VulkanContext* vkContext);

	// -------------------------------------------------------------------------------------------------------- //

	bool CreateBuffer(VulkanContext* vkContext, uint64 size, VkBufferUsageFlagBits usage,
		uint32 memoryPropertyFlags, bool bindOnCreate, VulkanBuffer* outBuffer);

	void DestroyBuffer(VulkanContext* vkContext, VulkanBuffer* buffer);

	bool ResizeBuffer(VulkanContext* vkContext, uint64 newSize, 
		VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool);

	void CopyBuffer(VulkanContext* vkContext, VkCommandPool pool, VkFence fence, VkQueue queue,
		VkBuffer source, uint64 sourceOffset, VkBuffer dest, uint64 destOffset, uint64 size);
	
	void LoadData(VulkanContext* vkContext, VulkanBuffer* buffer,
		uint64 offset, uint64 size, uint32 flags, const void* data);

	void BindBuffer(VulkanContext* vkContext, VulkanBuffer* buffer, VkDeviceSize memoryOffset);

	// -------------------------------------------------------------------------------------------------------- //

	void UploadDataRange(VulkanContext* vkContext, VkCommandPool pool, VkFence fence, VkQueue queue,
		VulkanBuffer* buffer, uint64 offset, uint64 size, const void* data);

	void FreeDataRange(VulkanContext* vkContext, VulkanBuffer* buffer, uint64 offset, uint64 size);
}
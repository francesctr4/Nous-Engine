#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanCommandBuffer.h"
#include "VulkanShaderUtils.h"
#include "VulkanDevice.h"

#include "MemoryManager.h"

bool NOUS_VulkanBuffer::CreateBuffers(VulkanContext* vkContext)
{
    VkMemoryPropertyFlagBits memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Vertex Buffer (VBO)
    const uint64 vertexBufferSize = sizeof(Vertex) * 1024 * 1024; // Placeholder

    if (!CreateBuffer(vkContext, vertexBufferSize,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        memoryPropertyFlags, true, &vkContext->objectVertexBuffer)) 
    {
        NOUS_ERROR("Error creating vertex buffer.");
        return false;
    }

    vkContext->geometryVertexOffset = 0;

    // Index Buffer (EBO)
    const uint64 indexBufferSize = sizeof(uint32) * 1024 * 1024; // Placeholder

    if (!CreateBuffer(vkContext, indexBufferSize,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        memoryPropertyFlags, true, &vkContext->objectIndexBuffer)) 
    {
        NOUS_ERROR("Error creating index buffer.");
        return false;
    }
    
    vkContext->geometryIndexOffset = 0;

	return true;
}

void NOUS_VulkanBuffer::DestroyBuffers(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Buffers...");

    DestroyBuffer(vkContext, &vkContext->objectIndexBuffer);
    DestroyBuffer(vkContext, &vkContext->objectVertexBuffer);
}

// -------------------------------------------------------------------------------------------------------- //

bool NOUS_VulkanBuffer::CreateBuffer(VulkanContext* vkContext, uint64 size, VkBufferUsageFlagBits usage, 
	uint32 memoryPropertyFlags, bool bindOnCreate, VulkanBuffer* outBuffer)
{
    MemoryManager::ZeroMemory(outBuffer, sizeof(VulkanBuffer));

    outBuffer->totalSize = size;
    outBuffer->usage = usage;
    outBuffer->memoryPropertyFlags = memoryPropertyFlags;

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue.

    VK_CHECK(vkCreateBuffer(vkContext->device.logicalDevice, &bufferCreateInfo, vkContext->allocator, &outBuffer->handle));

    // Gather memory requirements.
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(vkContext->device.logicalDevice, outBuffer->handle, &requirements);
   
    outBuffer->memoryIndex = FindMemoryIndex(vkContext->device.physicalDevice, requirements.memoryTypeBits, outBuffer->memoryPropertyFlags);
    
    if (outBuffer->memoryIndex == -1) 
    {
        NOUS_ERROR("Unable to create vulkan buffer because the required memory type index was not found.");
        return false;
    }

    // Allocate memory info
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    memoryAllocateInfo.allocationSize = requirements.size;
    memoryAllocateInfo.memoryTypeIndex = static_cast<uint32>(outBuffer->memoryIndex);

    // Allocate the memory.
    VkResult result = vkAllocateMemory(vkContext->device.logicalDevice, 
        &memoryAllocateInfo, vkContext->allocator, &outBuffer->memory);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("Unable to create vulkan buffer because the required memory allocation failed. Error: %i", result);
        return false;
    }

    if (bindOnCreate) 
    {
        BindBuffer(vkContext, outBuffer, 0);
    }

    return true;
}

void NOUS_VulkanBuffer::DestroyBuffer(VulkanContext* vkContext, VulkanBuffer* buffer)
{
    if (buffer->memory) 
    {
        vkFreeMemory(vkContext->device.logicalDevice, buffer->memory, vkContext->allocator);
        buffer->memory = 0;
    }

    if (buffer->handle) 
    {
        vkDestroyBuffer(vkContext->device.logicalDevice, buffer->handle, vkContext->allocator);
        buffer->handle = 0;
    }

    buffer->totalSize = 0;
    buffer->isLocked = false;
}

bool NOUS_VulkanBuffer::ResizeBuffer(VulkanContext* vkContext, uint64 newSize, 
	VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool)
{
    // Create new buffer.
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

    bufferCreateInfo.size = newSize;
    bufferCreateInfo.usage = buffer->usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // NOTE: Only used in one queue.

    VkBuffer newBuffer;
    VK_CHECK(vkCreateBuffer(vkContext->device.logicalDevice, &bufferCreateInfo, vkContext->allocator, &newBuffer));

    // Gather memory requirements.
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(vkContext->device.logicalDevice, newBuffer, &requirements);

    // Allocate memory info
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    memoryAllocateInfo.allocationSize = requirements.size;
    memoryAllocateInfo.memoryTypeIndex = static_cast<uint32>(buffer->memoryIndex);

    // Allocate the memory.
    VkDeviceMemory newMemory;

    VkResult result = vkAllocateMemory(vkContext->device.logicalDevice, &memoryAllocateInfo, vkContext->allocator, &newMemory);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("Unable to resize vulkan buffer because the required memory allocation failed. Error: %i", result);
        return false;
    }

    // Bind the new buffer's memory
    VK_CHECK(vkBindBufferMemory(vkContext->device.logicalDevice, newBuffer, newMemory, 0));

    // Copy over the data
    CopyBuffer(vkContext, pool, 0, queue, buffer->handle, 0, newBuffer, 0, buffer->totalSize);

    // Make sure anything potentially using these is finished.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // Destroy the old
    if (buffer->memory) 
    {
        vkFreeMemory(vkContext->device.logicalDevice, buffer->memory, vkContext->allocator);
        buffer->memory = 0;
    }

    if (buffer->handle) 
    {
        vkDestroyBuffer(vkContext->device.logicalDevice, buffer->handle, vkContext->allocator);
        buffer->handle = 0;
    }

    // Set new properties
    buffer->totalSize = newSize;
    buffer->memory = newMemory;
    buffer->handle = newBuffer;

    return true;
}

void NOUS_VulkanBuffer::CopyBuffer(VulkanContext* vkContext, VkCommandPool pool, VkFence fence, VkQueue queue, 
	VkBuffer source, uint64 sourceOffset, VkBuffer dest, uint64 destOffset, uint64 size)
{
    vkQueueWaitIdle(queue);

    // Create a one-time-use command buffer.
    VulkanCommandBuffer tempCommandBuffer;
    NOUS_VulkanCommandBuffer::CommandBufferAllocateAndBeginSingleTime(vkContext, pool, &tempCommandBuffer);

    // Prepare the copy command and add it to the command buffer.
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    copyRegion.srcOffset = sourceOffset;
    copyRegion.dstOffset = destOffset;

    vkCmdCopyBuffer(tempCommandBuffer.handle, source, dest, 1, &copyRegion);

    // Submit the buffer for execution and wait for it to complete.
    NOUS_VulkanCommandBuffer::CommandBufferEndAndFreeSingleTime(vkContext, pool, &tempCommandBuffer, queue);
}

void NOUS_VulkanBuffer::LoadBuffer(VulkanContext* vkContext, VulkanBuffer* buffer,
	uint64 offset, uint64 size, uint32 flags, const void* data)
{
    void* dataPtr;
    VK_CHECK(vkMapMemory(vkContext->device.logicalDevice, buffer->memory, offset, size, flags, &dataPtr));
    MemoryManager::CopyMemory(dataPtr, data, size);
    vkUnmapMemory(vkContext->device.logicalDevice, buffer->memory);
}

void NOUS_VulkanBuffer::BindBuffer(VulkanContext* vkContext, VulkanBuffer* buffer, VkDeviceSize memoryOffset)
{
    VK_CHECK(vkBindBufferMemory(vkContext->device.logicalDevice, buffer->handle, buffer->memory, memoryOffset));
}
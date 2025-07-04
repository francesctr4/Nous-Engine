#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanCommandBuffer.h"
#include "VulkanShaderUtils.h"
#include "VulkanDevice.h"

#include "MemoryManager.h"
#include "FreeList.h"

void CleanupFreelist(VulkanBuffer* buffer) 
{
    // ----- FREE LIST ----- //
    buffer->bufferFreelist->~Freelist();
    MemoryManager::Free(buffer->freelistBlock, buffer->freelistMemoryRequirement, MemoryManager::MemoryTag::RENDERER);

    buffer->freelistMemoryRequirement = 0;
    buffer->freelistBlock = nullptr;
}

bool NOUS_VulkanBuffer::CreateBuffers(VulkanContext* vkContext)
{
    VkMemoryPropertyFlagBits memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Geometry Vertex Buffer (VBO)
    const uint64 vertexBufferSize = sizeof(Vertex3D) * 1024 * 1024; // Placeholder

    if (!CreateBuffer(vkContext, vertexBufferSize,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        memoryPropertyFlags, true, &vkContext->objectVertexBuffer)) 
    {
        NOUS_ERROR("Error creating vertex buffer.");
        return false;
    }

    // Geometry Index Buffer (EBO)
    const uint64 indexBufferSize = sizeof(uint32) * 1024 * 1024; // Placeholder

    if (!CreateBuffer(vkContext, indexBufferSize,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        memoryPropertyFlags, true, &vkContext->objectIndexBuffer)) 
    {
        NOUS_ERROR("Error creating index buffer.");
        return false;
    }

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

    // ----- FREE LIST ----- //
    outBuffer->freelistMemoryRequirement = Freelist::GetMemoryRequirement(size);
    outBuffer->freelistBlock = MemoryManager::Allocate(outBuffer->freelistMemoryRequirement, MemoryManager::MemoryTag::RENDERER);
    outBuffer->bufferFreelist = new (&outBuffer->freelistBlock) Freelist(size, outBuffer->freelistBlock);

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue.

    VK_CHECK(vkCreateBuffer(vkContext->device.logicalDevice, &bufferCreateInfo, vkContext->allocator, &outBuffer->handle));

    // Gather memory requirements.
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(vkContext->device.logicalDevice, outBuffer->handle, &requirements);
   
    outBuffer->memoryIndex = NOUS_VulkanDevice::FindMemoryIndex(vkContext->device.physicalDevice, requirements.memoryTypeBits, outBuffer->memoryPropertyFlags);
    
    if (outBuffer->memoryIndex == -1) 
    {
        NOUS_ERROR("Unable to create vulkan buffer because the required memory type index was not found.");
        
        // ----- FREE LIST ----- //
        CleanupFreelist(outBuffer);

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
        
        // ----- FREE LIST ----- //
        CleanupFreelist(outBuffer);
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
    // ----- FREE LIST ----- //
    if (buffer->freelistBlock) 
    {
        CleanupFreelist(buffer);
    }

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
    // ----- FREE LIST ----- //
    if (newSize < buffer->totalSize) 
    {
        NOUS_ERROR("NOUS_VulkanBuffer::ResizeBuffer() requires that new size be larger than the old. Not doing this could lead to data loss.");
        return false;
    }

    // Resize the freelist first
    uint64 newMemoryRequirement = 0;

    // First call: Get memory requirement
    if (!buffer->bufferFreelist->Resize(newSize, &newMemoryRequirement, nullptr, nullptr)) 
    {
        NOUS_ERROR("NOUS_VulkanBuffer::ResizeBuffer(): Failed to calculate freelist memory requirement.");
        return false;
    }

    // Allocate new memory block
    void* newBlock = MemoryManager::Allocate(newMemoryRequirement, MemoryManager::MemoryTag::RENDERER);
    void* oldBlock = nullptr;

    // Second call: Perform actual resize
    if (!buffer->bufferFreelist->Resize(newSize, &newMemoryRequirement, newBlock, &oldBlock)) 
    {
        NOUS_ERROR("NOUS_VulkanBuffer::ResizeBuffer(): Failed to resize freelist.");
        MemoryManager::Free(newBlock, newMemoryRequirement, MemoryManager::MemoryTag::RENDERER);
        return false;
    }

    // Cleanup old memory and update buffer properties
    MemoryManager::Free(oldBlock, buffer->freelistMemoryRequirement, MemoryManager::MemoryTag::RENDERER);

    buffer->freelistMemoryRequirement = newMemoryRequirement;
    buffer->freelistBlock = newBlock;
    buffer->totalSize = newSize;

    // ----- END FREE LIST ----- //

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

void NOUS_VulkanBuffer::LoadData(VulkanContext* vkContext, VulkanBuffer* buffer,
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

void* NOUS_VulkanBuffer::LockMemory(VulkanContext* vkContext, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags)
{
    void* data;
    VK_CHECK(vkMapMemory(vkContext->device.logicalDevice, buffer->memory, offset, size, flags, &data));
    return data;
}

void NOUS_VulkanBuffer::UnlockMemory(VulkanContext* vkContext, VulkanBuffer* buffer)
{
    vkUnmapMemory(vkContext->device.logicalDevice, buffer->memory);
}

bool NOUS_VulkanBuffer::Allocate(VulkanBuffer* buffer, uint64 size, uint64* outOffset)
{
    // ----- FREE LIST ----- //
    if (!buffer || !size || !outOffset) 
    {
        NOUS_ERROR("NOUS_VulkanBuffer::Allocate() requires valid buffer, a nonzero size and valid pointer to hold offset.");
        return false;
    }

    return buffer->bufferFreelist->Allocate(size, outOffset);
}

bool NOUS_VulkanBuffer::Free(VulkanBuffer* buffer, uint64 size, uint64 offset)
{
    // ----- FREE LIST ----- //
    if (!buffer || !size) 
    {
        NOUS_ERROR("NOUS_VulkanBuffer::Free() requires valid buffer and a nonzero size.");
        return false;
    }

    return buffer->bufferFreelist->Free(size, offset);
}

// -------------------------------------------------------------------------------------------------------- //

bool NOUS_VulkanBuffer::UploadDataRange(VulkanContext* vkContext, VkCommandPool pool, VkFence fence, VkQueue queue, VulkanBuffer* buffer, uint64* outOffset, uint64 size, const void* data)
{
    // ----- FREE LIST ----- //
    // Allocate space in the buffer.
    if (!NOUS_VulkanBuffer::Allocate(buffer, size, outOffset))
    {
        NOUS_ERROR("NOUS_VulkanBuffer::UploadDataRange() failed to allocate from the given buffer!");
        return false;
    }

    // Create a host-visible staging buffer to upload to. Mark it as the source of the transfer.
    VkBufferUsageFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Create the staging buffer
    VulkanBuffer stagingBuffer;
    CreateBuffer(vkContext, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, flags, true, &stagingBuffer);

    // Load the data into the staging buffer.
    LoadData(vkContext, &stagingBuffer, 0, size, 0, data);

    // Perform the copy from staging to the device local buffer.
    CopyBuffer(vkContext, pool, fence, queue, stagingBuffer.handle, 0, buffer->handle, *outOffset, size);

    // Clean up the staging buffer.
    DestroyBuffer(vkContext, &stagingBuffer);

    return true;
}

void NOUS_VulkanBuffer::FreeDataRange(VulkanContext* vkContext, VulkanBuffer* buffer, uint64 offset, uint64 size)
{
    // ----- FREE LIST ----- //
    if (buffer) 
    {
        NOUS_VulkanBuffer::Free(buffer, size, offset);
    }
}
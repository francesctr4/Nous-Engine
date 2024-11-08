#include "VulkanCommandBuffer.h"

void NOUS_VulkanCommandBuffer::CommandBufferAllocate(VulkanContext* vkContext, VkCommandPool commandPool, 
	bool isPrimary, VulkanCommandBuffer* outCommandBuffer)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferFree(VulkanContext* vkContext, VkCommandPool pool, 
	VulkanCommandBuffer* commandBuffer)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferBegin(VulkanCommandBuffer* commandBuffer, bool isSingleUse, 
	bool isRenderpassContinue, bool isSimultaneousUse)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferEnd(VulkanCommandBuffer* commandBuffer)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(VulkanCommandBuffer* commandBuffer)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferReset(VulkanCommandBuffer* commandBuffer)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferAllocateAndBeginSingleTime(VulkanContext* vkContext, 
	VkCommandPool pool, VulkanCommandBuffer* outCommandBuffer)
{

}

void NOUS_VulkanCommandBuffer::CommandBufferEndAndFreeSingleTime(VulkanContext* vkContext, VkCommandPool pool, 
	VulkanCommandBuffer* commandBuffer, VkQueue queue)
{

}
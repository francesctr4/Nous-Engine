#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanCommandBuffer 
{
    bool CreateCommandBuffers(VulkanContext* vkContext);

    void DestroyCommandBuffers(VulkanContext* vkContext);

    // -------------------------------------------------------------------------------------------------------- //

    void CommandBufferAllocate(VulkanContext* vkContext, VkCommandPool commandPool,
        bool isPrimary, VulkanCommandBuffer* outCommandBuffer);

    void CommandBufferFree(VulkanContext* vkContext, VkCommandPool commandPool, VulkanCommandBuffer* commandBuffer);

    void CommandBufferBegin(VulkanCommandBuffer* commandBuffer, bool isSingleUse,
        bool isRenderpassContinue, bool isSimultaneousUse);

    void CommandBufferEnd(VulkanCommandBuffer* commandBuffer);

    void CommandBufferUpdateSubmitted(VulkanCommandBuffer* commandBuffer);

    void CommandBufferReset(VulkanCommandBuffer* commandBuffer);

    void CommandBufferAllocateAndBeginSingleTime(VulkanContext* vkContext,
        VkCommandPool commandPool, VulkanCommandBuffer* outCommandBuffer);

    void CommandBufferEndAndFreeSingleTime(VulkanContext* vkContext, VkCommandPool commandPool,
        VulkanCommandBuffer* commandBuffer, VkQueue queue);
}
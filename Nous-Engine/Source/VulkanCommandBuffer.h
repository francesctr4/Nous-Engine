#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanCommandBuffer 
{
    void CreateCommandBuffers(VulkanContext* vkContext);

    void DestroyCommandBuffers(VulkanContext* vkContext);

    // -------------------------------------------------------------------------------------------------------- //

    void CommandBufferAllocate(VulkanContext* vkContext, VkCommandPool commandPool,
        bool isPrimary, VulkanCommandBuffer* outCommandBuffer);

    void CommandBufferFree(VulkanContext* vkContext, VkCommandPool pool, VulkanCommandBuffer* commandBuffer);

    void CommandBufferBegin(VulkanCommandBuffer* commandBuffer, bool isSingleUse,
        bool isRenderpassContinue, bool isSimultaneousUse);

    void CommandBufferEnd(VulkanCommandBuffer* commandBuffer);

    void CommandBufferUpdateSubmitted(VulkanCommandBuffer* commandBuffer);

    void CommandBufferReset(VulkanCommandBuffer* commandBuffer);

    void CommandBufferAllocateAndBeginSingleTime(VulkanContext* vkContext,
        VkCommandPool pool, VulkanCommandBuffer* outCommandBuffer);

    void CommandBufferEndAndFreeSingleTime(VulkanContext* vkContext, VkCommandPool pool,
        VulkanCommandBuffer* commandBuffer, VkQueue queue);
}
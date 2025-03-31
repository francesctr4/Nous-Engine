#pragma once

#include "VulkanTypes.inl"

enum RenderpassClearFlag 
{
    NO_CLEAR,

    COLOR_BUFFER,
    DEPTH_BUFFER,
    STENCIL_BUFFER
};

namespace NOUS_VulkanRenderpass 
{
    bool CreateRenderpass(
        VulkanContext* vkContext,
        VulkanRenderpass* outRenderpass,
        float4 renderArea, float4 clearColor,
        float depth, uint32 stencil, uint8 clearFlags,
        bool prevPass, bool nextPass);

	bool CreateOffscreenRenderpass(VulkanContext* vkContext);

    void DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass);

    void BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer);

    void EndRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass);
}
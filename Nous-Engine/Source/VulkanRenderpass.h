#pragma once

#include "VulkanTypes.inl"

bool CreateRenderpass(
    VulkanContext* vkContext,
    VulkanRenderpass* outRenderpass,
    float32 x, float32 y, float32 w, float32 h,
    float32 r, float32 g, float32 b, float32 a,
    float32 depth, uint32 stencil);

void DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass);

void BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer);

void EndRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass);

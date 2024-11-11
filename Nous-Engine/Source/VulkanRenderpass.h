#pragma once

#include "VulkanTypes.inl"

bool CreateRenderpass(
    VulkanContext* vkContext,
    VulkanRenderpass* outRenderpass,
    float x, float y, float w, float h,
    float r, float g, float b, float a,
    float depth, uint32 stencil);

void DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass);

void BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer);

void EndRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass);

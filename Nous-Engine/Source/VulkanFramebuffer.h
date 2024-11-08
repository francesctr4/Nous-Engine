#pragma once

#include "VulkanTypes.inl"

void vulkan_framebuffer_create(
    vulkan_context* context,
    vulkan_renderpass* renderpass,
    u32 width,
    u32 height,
    u32 attachment_count,
    VkImageView* attachments,
    vulkan_framebuffer* out_framebuffer);

void vulkan_framebuffer_destroy(vulkan_context* context, vulkan_framebuffer* framebuffer);

void regenerate_framebuffers();
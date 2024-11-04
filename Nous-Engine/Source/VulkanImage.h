#pragma once

#include "VulkanTypes.inl"

void CreateVulkanImage(VulkanContext* context, VkImageType imageType, uint32 width, uint32 height,
    uint32 mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, 
    VkImageUsageFlags usage, VkMemoryPropertyFlags memoryFlags, bool createView, 
    VkImageAspectFlags viewAspectFlags, VulkanImage* outImage);

void CreateVulkanImageView(VulkanContext* context, VkFormat format, 
    VulkanImage* image, VkImageAspectFlags aspectFlags, uint32 mipLevels);

void DestroyVulkanImage(VulkanContext* context, VulkanImage* image);
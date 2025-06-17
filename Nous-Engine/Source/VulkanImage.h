#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanImage 
{
    void CreateVulkanImage(VulkanContext* vkContext, VkImageType imageType, uint32 width, uint32 height,
        uint32 mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags memoryFlags, bool createView,
        VkImageAspectFlags viewAspectFlags, VulkanImage* outImage);

    void CreateVulkanImageView(VulkanContext* vkContext, VkFormat format,
        VulkanImage* image, VkImageAspectFlags aspectFlags, uint32 mipLevels);

    /**
     * Transitions the provided image from old_layout to new_layout.
     */
    void TransitionVulkanImageLayout(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer,
        VulkanImage* image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    /**
     * Copies data in buffer to provided image.
     * @param context The Vulkan context.
     * @param image The image to copy the buffer's data to.
     * @param buffer The buffer whose data will be copied.
     */
    void CopyBufferToVulkanImage(VulkanContext* vkContext, VulkanImage* image,
        VkBuffer buffer, VulkanCommandBuffer* commandBuffer);

    void DestroyVulkanImage(VulkanContext* vkContext, VulkanImage* image);
}
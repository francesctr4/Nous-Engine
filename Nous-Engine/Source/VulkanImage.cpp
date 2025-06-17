#include "VulkanImage.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include "Logger.h"
#include "MemoryManager.h"

void NOUS_VulkanImage::CreateVulkanImage(VulkanContext* vkContext, VkImageType imageType, uint32 width, uint32 height,
    uint32 mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, 
    VkImageUsageFlags usage, VkMemoryPropertyFlags memoryFlags, bool createView, 
    VkImageAspectFlags viewAspectFlags, VulkanImage* outImage)
{
	outImage->width = width;
	outImage->height = height;

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;                           // TODO: Support configurable depth.
    imageCreateInfo.mipLevels = mipLevels;                      // TODO: Support mip mapping
    imageCreateInfo.arrayLayers = 1;                            // TODO: Support number of layers in the image.

    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;    // TODO: Configurable sharing mode.
    imageCreateInfo.samples = numSamples;                       // TODO: Configurable sample count.
    imageCreateInfo.flags = 0;

    VK_CHECK(vkCreateImage(vkContext->device.logicalDevice, &imageCreateInfo, vkContext->allocator, &outImage->handle));

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vkContext->device.logicalDevice, outImage->handle, &memoryRequirements);

    int32 memoryType = NOUS_VulkanDevice::FindMemoryIndex(vkContext->device.physicalDevice, memoryRequirements.memoryTypeBits, memoryFlags);
    if (memoryType == -1) 
    {
        NOUS_ERROR("Required memory type not found. Image not valid.");
    }

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryType;

    VK_CHECK(vkAllocateMemory(vkContext->device.logicalDevice, &memoryAllocateInfo, vkContext->allocator, &outImage->memory));

    VK_CHECK(vkBindImageMemory(vkContext->device.logicalDevice, outImage->handle, outImage->memory, 0));  // TODO: configurable memory offset.

    if (createView) 
    {
        outImage->view = 0;
        CreateVulkanImageView(vkContext, format, outImage, viewAspectFlags, mipLevels);
    }
}

void NOUS_VulkanImage::CreateVulkanImageView(VulkanContext* vkContext, VkFormat format,
	VulkanImage* image, VkImageAspectFlags aspectFlags, uint32 mipLevels)
{
    VkImageViewCreateInfo imageViewCreateInfo{}; 
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    imageViewCreateInfo.image = image->handle;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;  // TODO: Make configurable.
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;

    // TODO: Make configurable
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(vkContext->device.logicalDevice, &imageViewCreateInfo, vkContext->allocator, &image->view));
}

void NOUS_VulkanImage::TransitionVulkanImageLayout(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer,
    VulkanImage* image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;

    imageMemoryBarrier.srcQueueFamilyIndex = vkContext->device.graphicsQueueIndex;
    imageMemoryBarrier.dstQueueFamilyIndex = vkContext->device.graphicsQueueIndex;

    imageMemoryBarrier.image = image->handle;

    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // Don't care about the old layout - transition to optimal layout (for the underlying implementation).
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        // Don't care what stage the pipeline is in at the start.
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        // Used for copying
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
    {
        // Transitioning from a transfer destination layout to a shader-readonly layout.
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // From a copying stage to...
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        // The fragment stage.
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else 
    {
        NOUS_FATAL("Unsupported Layout Transition!");
        return;
    }

    vkCmdPipelineBarrier(commandBuffer->handle,
        sourceStage, destinationStage,
        0,
        0, 0,
        0, 0,
        1, &imageMemoryBarrier);
}

void NOUS_VulkanImage::CopyBufferToVulkanImage(VulkanContext* vkContext, VulkanImage* image,
    VkBuffer buffer, VulkanCommandBuffer* commandBuffer)
{
    // Region to copy
    VkBufferImageCopy bufferImageCopyRegion;
    MemoryManager::ZeroMemory(&bufferImageCopyRegion, sizeof(VkBufferImageCopy));

    bufferImageCopyRegion.bufferOffset = 0;
    bufferImageCopyRegion.bufferRowLength = 0;
    bufferImageCopyRegion.bufferImageHeight = 0;

    bufferImageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopyRegion.imageSubresource.mipLevel = 0;
    bufferImageCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferImageCopyRegion.imageSubresource.layerCount = 1;

    bufferImageCopyRegion.imageExtent.width = image->width;
    bufferImageCopyRegion.imageExtent.height = image->height;
    bufferImageCopyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandBuffer->handle, buffer, image->handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopyRegion);
}

void NOUS_VulkanImage::DestroyVulkanImage(VulkanContext* vkContext, VulkanImage* image)
{
    if (image->view) 
    {
        vkDestroyImageView(vkContext->device.logicalDevice, image->view, vkContext->allocator);
        image->view = 0;
    }

    if (image->memory) 
    {
        vkFreeMemory(vkContext->device.logicalDevice, image->memory, vkContext->allocator);
        image->memory = 0;
    }

    if (image->handle) 
    {
        vkDestroyImage(vkContext->device.logicalDevice, image->handle, vkContext->allocator);
        image->handle = 0;
    }
}
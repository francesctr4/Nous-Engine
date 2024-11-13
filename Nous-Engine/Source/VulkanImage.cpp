#include "VulkanImage.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include "Logger.h"
#include "MemoryManager.h"

void CreateVulkanImage(VulkanContext* context, VkImageType imageType, uint32 width, uint32 height, 
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

    VK_CHECK(vkCreateImage(context->device.logicalDevice, &imageCreateInfo, context->allocator, &outImage->handle));

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(context->device.logicalDevice, outImage->handle, &memoryRequirements);

    int32 memoryType = FindMemoryType(context->device.physicalDevice, memoryRequirements.memoryTypeBits, memoryFlags);
    if (memoryType == -1) 
    {
        NOUS_ERROR("Required memory type not found. Image not valid.");
    }

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryType;

    VK_CHECK(vkAllocateMemory(context->device.logicalDevice, &memoryAllocateInfo, context->allocator, &outImage->memory));

    VK_CHECK(vkBindImageMemory(context->device.logicalDevice, outImage->handle, outImage->memory, 0));  // TODO: configurable memory offset.

    if (createView) 
    {
        outImage->view = 0;
        CreateVulkanImageView(context, format, outImage, viewAspectFlags, mipLevels);
    }
}

void CreateVulkanImageView(VulkanContext* context, VkFormat format, 
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

    VK_CHECK(vkCreateImageView(context->device.logicalDevice, &imageViewCreateInfo, context->allocator, &image->view));
};

void DestroyVulkanImage(VulkanContext* context, VulkanImage* image)
{
    if (image->view) 
    {
        vkDestroyImageView(context->device.logicalDevice, image->view, context->allocator);
        image->view = 0;
    }

    if (image->memory) 
    {
        vkFreeMemory(context->device.logicalDevice, image->memory, context->allocator);
        image->memory = 0;
    }

    if (image->handle) 
    {
        vkDestroyImage(context->device.logicalDevice, image->handle, context->allocator);
        image->handle = 0;
    }
}
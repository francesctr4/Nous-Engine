#include "VulkanImGuiResources.h"
#include "VulkanImage.h"
#include "VulkanUtils.h"
#include "VulkanRenderpass.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"

#include "ImGui.h"

void NOUS_ImGuiVulkanResources::CreateImGuiVulkanResources(VulkanContext* vkContext)
{
	CreateImGuiDescriptorPool(vkContext);
	CreateImGuiTextureSampler(vkContext);
	//CreateImGuiImages(vkContext, &vkContext->swapChain);
	CreateViewportImages(vkContext);
	CreateViewportImageViews(vkContext);
	CreateDepthResources(vkContext);
}

void NOUS_ImGuiVulkanResources::DestroyImGuiVulkanResources(VulkanContext* vkContext)
{
	//NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->imGuiResources.viewportRenderPass);

	//for (auto& image : vkContext->imGuiResources.m_ViewportImages)
	//{
	//	DestroyVulkanImage(vkContext, &image);
	//}

	DestroyDepthResources(vkContext);

	for (int i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		if (vkContext->imGuiResources.m_ViewportImageViews[i])
		{
			vkDestroyImageView(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportImageViews[i], vkContext->allocator);
			vkContext->imGuiResources.m_ViewportImageViews[i] = 0;
		}

		if (vkContext->imGuiResources.m_ViewportDstImageMemory[i])
		{
			vkFreeMemory(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportDstImageMemory[i], vkContext->allocator);
			vkContext->imGuiResources.m_ViewportDstImageMemory[i] = 0;
		}

		if (vkContext->imGuiResources.m_ViewportImages[i])
		{
			vkDestroyImage(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportImages[i], vkContext->allocator);
			vkContext->imGuiResources.m_ViewportImages[i] = 0;
		}

	}
	vkDestroySampler(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportTextureSampler, vkContext->allocator);
	vkDestroyDescriptorPool(vkContext->device.logicalDevice, vkContext->imGuiResources.descriptorPool, vkContext->allocator);
}

void NOUS_ImGuiVulkanResources::RecreateImGuiVulkanResources(VulkanContext* vkContext)
{
	// Destroy all

	DestroyDepthResources(vkContext);

	for (int i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		if (vkContext->imGuiResources.m_ViewportImageViews[i])
		{
			vkDestroyImageView(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportImageViews[i], vkContext->allocator);
			vkContext->imGuiResources.m_ViewportImageViews[i] = 0;
		}

		if (vkContext->imGuiResources.m_ViewportDstImageMemory[i])
		{
			vkFreeMemory(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportDstImageMemory[i], vkContext->allocator);
			vkContext->imGuiResources.m_ViewportDstImageMemory[i] = 0;
		}

		if (vkContext->imGuiResources.m_ViewportImages[i])
		{
			vkDestroyImage(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportImages[i], vkContext->allocator);
			vkContext->imGuiResources.m_ViewportImages[i] = 0;
		}

	}

	for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImageViews.size(); ++i)
	{
		ImGui_ImplVulkan_RemoveTexture(vkContext->imGuiResources.m_ViewportDescriptorSets[i]);
	}

	// Recreate all

	CreateViewportImages(vkContext);
	CreateViewportImageViews(vkContext);

	CreateDepthResources(vkContext);

	for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImageViews.size(); ++i)
	{
		vkContext->imGuiResources.m_ViewportDescriptorSets[i] = ImGui_ImplVulkan_AddTexture(vkContext->imGuiResources.m_ViewportTextureSampler, vkContext->imGuiResources.m_ViewportImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
}

// ----------------------------------------------------------------------------------- //

void NOUS_ImGuiVulkanResources::CreateImGuiDescriptorPool(VulkanContext* vkContext)
{
	std::array<VkDescriptorPoolSize, 11> descriptorPoolSizes = 
	{
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

	descriptorPoolCreateInfo.pNext = nullptr; // No extensions, so null
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	descriptorPoolCreateInfo.maxSets = 1000 * descriptorPoolSizes.size();
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	VK_CHECK(vkCreateDescriptorPool(vkContext->device.logicalDevice, &descriptorPoolCreateInfo, vkContext->allocator, &vkContext->imGuiResources.descriptorPool));
}

//void NOUS_ImGuiVulkanResources::CreateImGuiImages(VulkanContext* vkContext, VulkanSwapChain* swapChain)
//{
//	uint32 swapChainImageCount = vkContext->swapChain.swapChainImages.size();
//
//	vkContext->imGuiResources.m_ViewportImages.resize(swapChainImageCount);
//
//	for (uint32 i = 0; i < swapChainImageCount; ++i)
//	{
//		CreateVulkanImage(
//			vkContext,
//			VK_IMAGE_TYPE_2D,
//			swapChain->swapChainExtent.width,
//			swapChain->swapChainExtent.height,
//			1,
//			vkContext->device.msaaSamples,
//			vkContext->device.colorFormat,
//			VK_IMAGE_TILING_OPTIMAL,
//			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
//			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//			true,
//			VK_IMAGE_ASPECT_COLOR_BIT,
//			&vkContext->imGuiResources.m_ViewportImages[i]);
//	}
//}

//void NOUS_ImGuiVulkanResources::CreateImGuiRenderPass(VulkanContext* vkContext)
//{
//	NOUS_ASSERT(NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->imGuiResources.viewportRenderPass,
//		float4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
//		float4(0.0f, 0.0f, 0.5f, 1.0f),
//		1.0f,
//		0,
//		RenderpassClearFlag::NO_CLEAR,
//		false, false));
//}

//void NOUS_ImGuiVulkanResources::CreateImGuiPipeline(VulkanContext* vkContext)
//{
//	
//}

void NOUS_ImGuiVulkanResources::CreateImGuiFramebuffers(VulkanContext* vkContext)
{
	uint32 imageCount = vkContext->swapChain.swapChainFramebuffers.size();

	for (uint16 i = 0; i < imageCount; ++i)
	{
		// World Attachments

		std::array<VkImageView, 3> attachments = { vkContext->swapChain.colorAttachment.view, vkContext->swapChain.depthAttachment.view, vkContext->swapChain.swapChainImageViews[i] };

		VkFramebufferCreateInfo worldFramebufferCreateInfo{};
		worldFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		worldFramebufferCreateInfo.renderPass = vkContext->mainRenderpass.handle;
		worldFramebufferCreateInfo.attachmentCount = static_cast<uint32>(attachments.size());
		worldFramebufferCreateInfo.pAttachments = attachments.data();
		worldFramebufferCreateInfo.width = vkContext->framebufferWidth;
		worldFramebufferCreateInfo.height = vkContext->framebufferHeight;
		worldFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &worldFramebufferCreateInfo,
			vkContext->allocator, &vkContext->worldFramebuffers[i]));

		// UI Attachments

		VkFramebufferCreateInfo uiFramebufferCreateInfo{};
		uiFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		uiFramebufferCreateInfo.renderPass = vkContext->uiRenderpass.handle;
		uiFramebufferCreateInfo.attachmentCount = static_cast<uint32>(attachments.size());
		uiFramebufferCreateInfo.pAttachments = attachments.data();
		uiFramebufferCreateInfo.width = vkContext->framebufferWidth;
		uiFramebufferCreateInfo.height = vkContext->framebufferHeight;
		uiFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &uiFramebufferCreateInfo,
			vkContext->allocator, &vkContext->swapChain.swapChainFramebuffers[i]));
	}
}

//void NOUS_ImGuiVulkanResources::CreateImGuiCommandPool(VulkanContext* vkContext)
//{
//	VkCommandPoolCreateInfo commandPoolCreateInfo{};
//	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
//
//	commandPoolCreateInfo.queueFamilyIndex = vkContext->device.graphicsQueueIndex;
//	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
//
//	VK_CHECK(vkCreateCommandPool(vkContext->device.logicalDevice, &commandPoolCreateInfo, vkContext->allocator, &vkContext->imGuiResources.viewportCommandPool));
//}
//
//void NOUS_ImGuiVulkanResources::CreateImGuiCommandBuffers(VulkanContext* vkContext)
//{
//	// Resize command buffers to match the swap chain image count
//	vkContext->imGuiResources.viewportCommandBuffers.resize(vkContext->swapChain.swapChainImageViews.size());
//
//	for (size_t i = 0; i < vkContext->imGuiResources.viewportCommandBuffers.size(); ++i) 
//	{
//		VulkanCommandBuffer& commandBuffer = vkContext->imGuiResources.viewportCommandBuffers[i];
//
//		// Allocate and initialize the command buffer
//		NOUS_VulkanCommandBuffer::CommandBufferAllocate(vkContext, vkContext->imGuiResources.viewportCommandPool, true, &commandBuffer);
//
//		// Begin recording
//		NOUS_VulkanCommandBuffer::CommandBufferBegin(&commandBuffer, false, false, false);
//
//		// Render pass setup
//		VkRenderPassBeginInfo renderPassInfo{};
//		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//		renderPassInfo.renderPass = vkContext->imGuiResources.viewportRenderPass.handle;
//		renderPassInfo.framebuffer = vkContext->imGuiResources.m_ViewportFramebuffers[i];
//		renderPassInfo.renderArea.offset = { 0, 0 };
//		renderPassInfo.renderArea.extent = vkContext->swapChain.swapChainExtent;
//
//		VkClearValue clearColor = { {0.0f, 0.0f, 0.0f, 1.0f} };
//		renderPassInfo.clearValueCount = 1;
//		renderPassInfo.pClearValues = &clearColor;
//
//		vkCmdBeginRenderPass(commandBuffer.handle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
//
//		// Bind pipeline and ImGui draw commands
//		vkCmdBindPipeline(commandBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->imGuiResources.viewportPipeline.handle);
//		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.handle);
//
//		// End render pass and recording
//		vkCmdEndRenderPass(commandBuffer.handle);
//		NOUS_VulkanCommandBuffer::CommandBufferEnd(&commandBuffer);
//	}
//}

void NOUS_ImGuiVulkanResources::CreateImGuiTextureSampler(VulkanContext* vkContext)
{
	// Create a sampler for the texture
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// TODO: These filters should be configurable.
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.maxAnisotropy = 16;

	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.0f;

	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;

	VK_CHECK(vkCreateSampler(vkContext->device.logicalDevice, &samplerCreateInfo, 
		vkContext->allocator, &vkContext->imGuiResources.m_ViewportTextureSampler));
}

//void NOUS_ImGuiVulkanResources::CreateImGuiDescriptorSets(VulkanContext* vkContext)
//{
//	vkContext->imGuiResources.m_ViewportDescriptorSets.resize(vkContext->imGuiResources.m_ViewportImages.size());
//
//	for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
//	{
//		vkContext->imGuiResources.m_ViewportDescriptorSets[i] = ImGui_ImplVulkan_AddTexture(vkContext->imGuiResources.m_ViewportTextureSampler, vkContext->imGuiResources.m_ViewportImages[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//	}
//}

//void NOUS_ImGuiVulkanResources::RenderImGuiDrawData(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderPass)
//{
//	// Begin render pass
//	NOUS_VulkanRenderpass::BeginRenderpass(commandBuffer, renderPass, vkContext->imGuiResources.m_ViewportFramebuffers[vkContext->imageIndex]);
//
//	// Render ImGui
//	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer->handle);
//
//	// End render pass
//	NOUS_VulkanRenderpass::EndRenderpass(commandBuffer, renderPass);
//}

void NOUS_ImGuiVulkanResources::InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.srcAccessMask = srcAccessMask;
	imageMemoryBarrier.dstAccessMask = dstAccessMask;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}

VkCommandBuffer NOUS_ImGuiVulkanResources::BeginSingleTimeCommands(VulkanContext* vkContext, const VkCommandPool& cmdPool)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = cmdPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(vkContext->device.logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void NOUS_ImGuiVulkanResources::EndSingleTimeCommands(VulkanContext* vkContext, VkCommandBuffer commandBuffer, const VkCommandPool& cmdPool)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(vkContext->device.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vkContext->device.graphicsQueue);

	vkFreeCommandBuffers(vkContext->device.logicalDevice, cmdPool, 1, &commandBuffer);
}

void NOUS_ImGuiVulkanResources::CreateViewportImages(VulkanContext* vkContext)
{
	vkContext->imGuiResources.m_ViewportImages.resize(vkContext->swapChain.swapChainImages.size());
	vkContext->imGuiResources.m_ViewportDstImageMemory.resize(vkContext->swapChain.swapChainImages.size());

	for (uint32_t i = 0; i < vkContext->swapChain.swapChainImages.size(); i++)
	{
		// Create the linear tiled destination image to copy to and to read the memory from
		VkImageCreateInfo imageCreateCI{};
		imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
		// Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
		imageCreateCI.format = VK_FORMAT_B8G8R8A8_SRGB;
		imageCreateCI.extent.width = vkContext->swapChain.swapChainExtent.width;
		imageCreateCI.extent.height = vkContext->swapChain.swapChainExtent.height;
		imageCreateCI.extent.depth = 1;
		imageCreateCI.arrayLayers = 1;
		imageCreateCI.mipLevels = 1;
		imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
		imageCreateCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		// Create the image
		// VkImage dstImage;
		vkCreateImage(vkContext->device.logicalDevice, &imageCreateCI, nullptr, &vkContext->imGuiResources.m_ViewportImages[i]);
		// Create memory to back up the image
		VkMemoryRequirements memRequirements;
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		// VkDeviceMemory dstImageMemory;
		vkGetImageMemoryRequirements(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportImages[i], &memRequirements);
		memAllocInfo.allocationSize = memRequirements.size;
		// Memory must be host visible to copy from
		memAllocInfo.memoryTypeIndex = FindMemoryIndex(vkContext->device.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkAllocateMemory(vkContext->device.logicalDevice, &memAllocInfo, nullptr, &vkContext->imGuiResources.m_ViewportDstImageMemory[i]);
		vkBindImageMemory(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportImages[i], vkContext->imGuiResources.m_ViewportDstImageMemory[i], 0);

		VkCommandBuffer copyCmd = BeginSingleTimeCommands(vkContext, vkContext->device.graphicsCommandPool);

		InsertImageMemoryBarrier(
			copyCmd,
			vkContext->imGuiResources.m_ViewportImages[i],
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		EndSingleTimeCommands(vkContext, copyCmd, vkContext->device.graphicsCommandPool);
	}
}

VkImageView NOUS_ImGuiVulkanResources::CreateImageView(VulkanContext* vkContext, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(vkContext->device.logicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}

	return imageView;
}

void NOUS_ImGuiVulkanResources::CreateViewportImageViews(VulkanContext* vkContext)
{
	vkContext->imGuiResources.m_ViewportImageViews.resize(vkContext->imGuiResources.m_ViewportImages.size());

	for (uint32_t i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); i++)
	{
		vkContext->imGuiResources.m_ViewportImageViews[i] = CreateImageView(vkContext, vkContext->imGuiResources.m_ViewportImages[i], VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void NOUS_ImGuiVulkanResources::CreateDepthResources(VulkanContext* vkContext)
{
	// Depth resources
	vkContext->device.depthFormat = FindDepthFormat(vkContext->device.physicalDevice);

	// Create depth image and its view.
	CreateVulkanImage(
		vkContext,
		VK_IMAGE_TYPE_2D,
		vkContext->framebufferWidth,  // Use framebuffer dimensions, NOT swapchain
		vkContext->framebufferHeight,
		1,  // Mip levels
		VK_SAMPLE_COUNT_1_BIT,
		vkContext->device.depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		true,  // Create view
		VK_IMAGE_ASPECT_DEPTH_BIT,  // Use VK_IMAGE_ASPECT_DEPTH_BIT | STENCIL if needed
		&vkContext->imGuiResources.m_ViewportDepthAttachment
	);
}

void NOUS_ImGuiVulkanResources::DestroyDepthResources(VulkanContext* vkContext)
{
	DestroyVulkanImage(vkContext, &vkContext->imGuiResources.m_ViewportDepthAttachment);
}

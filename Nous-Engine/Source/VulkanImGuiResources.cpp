#include "VulkanImGuiResources.h"
#include "VulkanImage.h"
#include "VulkanUtils.h"
#include "VulkanRenderpass.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandBuffer.h"

#include "ImGui.h"

void NOUS_ImGuiVulkanResources::CreateImGuiVulkanResources(VulkanContext* vkContext)
{
	CreateImGuiDescriptorPool(vkContext);
	CreateImGuiImages(vkContext, &vkContext->swapChain);
	CreateImGuiRenderPass(vkContext);
}

void NOUS_ImGuiVulkanResources::DestroyImGuiVulkanResources(VulkanContext* vkContext)
{
	DestroyRenderpass(vkContext, &vkContext->imGuiResources.viewportRenderPass);

	for (auto& image : vkContext->imGuiResources.viewportImages)
	{
		DestroyVulkanImage(vkContext, &image);
	}
	
	vkDestroyDescriptorPool(vkContext->device.logicalDevice, vkContext->imGuiResources.descriptorPool, vkContext->allocator);
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

void NOUS_ImGuiVulkanResources::CreateImGuiImages(VulkanContext* vkContext, VulkanSwapChain* swapChain)
{
	uint32 swapChainImageCount = vkContext->swapChain.swapChainImages.size();

	vkContext->imGuiResources.viewportImages.resize(swapChainImageCount);

	for (uint32 i = 0; i < swapChainImageCount; ++i)
	{
		CreateVulkanImage(
			vkContext,
			VK_IMAGE_TYPE_2D,
			swapChain->swapChainExtent.width,
			swapChain->swapChainExtent.height,
			1,
			vkContext->device.msaaSamples,
			vkContext->device.colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true,
			VK_IMAGE_ASPECT_COLOR_BIT,
			&vkContext->imGuiResources.viewportImages[i]);
	}
}

void NOUS_ImGuiVulkanResources::CreateImGuiRenderPass(VulkanContext* vkContext)
{
	NOUS_ASSERT(CreateRenderpass(vkContext, &vkContext->imGuiResources.viewportRenderPass,
		0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight,
		0.0f, 0.0f, 0.5f, 1.0f,
		1.0f,
		0));
}

void NOUS_ImGuiVulkanResources::CreateImGuiPipeline(VulkanContext* vkContext)
{
	
}

void NOUS_ImGuiVulkanResources::CreateImGuiFramebuffers(VulkanContext* vkContext)
{
	vkContext->imGuiResources.viewportFramebuffers.resize(vkContext->swapChain.swapChainImageViews.size());

	for (uint16 i = 0; i < vkContext->swapChain.swapChainFramebuffers.size(); ++i)
	{
		// TODO: make this dynamic based on the currently configured attachments
		std::array<VkImageView, 3> attachments = { vkContext->swapChain.colorAttachment.view, vkContext->swapChain.depthAttachment.view, vkContext->swapChain.swapChainImageViews[i] };

		NOUS_VulkanFramebuffer::CreateVulkanFramebuffer(vkContext, &vkContext->imGuiResources.viewportRenderPass, vkContext->framebufferWidth, vkContext->framebufferHeight,
			attachments.size(), attachments.data(), &vkContext->imGuiResources.viewportFramebuffers[i]);
	}
}

void NOUS_ImGuiVulkanResources::CreateImGuiCommandPool(VulkanContext* vkContext)
{
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	commandPoolCreateInfo.queueFamilyIndex = vkContext->device.graphicsQueueIndex;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(vkContext->device.logicalDevice, &commandPoolCreateInfo, vkContext->allocator, &vkContext->imGuiResources.viewportCommandPool));
}

void NOUS_ImGuiVulkanResources::CreateImGuiCommandBuffers(VulkanContext* vkContext)
{
	// Resize command buffers to match the swap chain image count
	vkContext->imGuiResources.viewportCommandBuffers.resize(vkContext->swapChain.swapChainImageViews.size());

	for (size_t i = 0; i < vkContext->imGuiResources.viewportCommandBuffers.size(); ++i) 
	{
		VulkanCommandBuffer& commandBuffer = vkContext->imGuiResources.viewportCommandBuffers[i];

		// Allocate and initialize the command buffer
		NOUS_VulkanCommandBuffer::CommandBufferAllocate(vkContext, vkContext->imGuiResources.viewportCommandPool, true, &commandBuffer);

		// Begin recording
		NOUS_VulkanCommandBuffer::CommandBufferBegin(&commandBuffer, false, false, false);

		// Render pass setup
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vkContext->imGuiResources.viewportRenderPass.handle;
		renderPassInfo.framebuffer = vkContext->imGuiResources.viewportFramebuffers[i].handle;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = vkContext->swapChain.swapChainExtent;

		VkClearValue clearColor = { {0.0f, 0.0f, 0.0f, 1.0f} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer.handle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind pipeline and ImGui draw commands
		vkCmdBindPipeline(commandBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->imGuiResources.viewportPipeline.handle);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.handle);

		// End render pass and recording
		vkCmdEndRenderPass(commandBuffer.handle);
		NOUS_VulkanCommandBuffer::CommandBufferEnd(&commandBuffer);
	}
}

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
		vkContext->allocator, &vkContext->imGuiResources.textureSampler));
}

void NOUS_ImGuiVulkanResources::CreateImGuiDescriptorSets(VulkanContext* vkContext)
{
	vkContext->imGuiResources.descriptorSets.resize(vkContext->imGuiResources.viewportImages.size());

	for (uint32 i = 0; i < vkContext->imGuiResources.viewportImages.size(); ++i)
	{
		vkContext->imGuiResources.descriptorSets[i] = ImGui_ImplVulkan_AddTexture(vkContext->imGuiResources.textureSampler, vkContext->imGuiResources.viewportImages[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
}

void NOUS_ImGuiVulkanResources::RenderImGuiDrawData(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderPass)
{
	// Begin render pass
	BeginRenderpass(commandBuffer, renderPass, vkContext->imGuiResources.viewportFramebuffers[vkContext->imageIndex].handle);

	// Render ImGui
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer->handle);

	// End render pass
	EndRenderpass(commandBuffer, renderPass);
}

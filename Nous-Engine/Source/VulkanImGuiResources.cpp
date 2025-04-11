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
	CreateViewportImages(vkContext);
	CreateDepthResources(vkContext);
}

void NOUS_ImGuiVulkanResources::DestroyImGuiVulkanResources(VulkanContext* vkContext)
{
	DestroyVulkanImage(vkContext, &vkContext->imGuiResources.m_ViewportDepthAttachment);

	for (int i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		DestroyVulkanImage(vkContext, &vkContext->imGuiResources.m_ViewportImages[i]);
	}

	vkDestroySampler(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportTextureSampler, vkContext->allocator);
	vkDestroyDescriptorPool(vkContext->device.logicalDevice, vkContext->imGuiResources.descriptorPool, vkContext->allocator);
}

void NOUS_ImGuiVulkanResources::RecreateImGuiVulkanResources(VulkanContext* vkContext)
{
	// Destroy all

	DestroyViewportDescriptorSets(vkContext);

	DestroyVulkanImage(vkContext, &vkContext->imGuiResources.m_ViewportDepthAttachment);

	for (int i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		DestroyVulkanImage(vkContext, &vkContext->imGuiResources.m_ViewportImages[i]);
	}

	// Recreate all

	CreateViewportImages(vkContext);
	CreateDepthResources(vkContext);

	CreateViewportDescriptorSets(vkContext);
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

void NOUS_ImGuiVulkanResources::CreateViewportImages(VulkanContext* vkContext)
{
	vkContext->imGuiResources.m_ViewportImages.resize(vkContext->swapChain.swapChainImages.size());

	for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		// Create depth image and its view.
		CreateVulkanImage(
			vkContext,
			VK_IMAGE_TYPE_2D,
			vkContext->framebufferWidth,  // Use framebuffer dimensions, NOT swapchain
			vkContext->framebufferHeight,
			1,  // Mip levels
			VK_SAMPLE_COUNT_1_BIT,
			vkContext->device.colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true,  // Create view
			VK_IMAGE_ASPECT_COLOR_BIT,  // Use VK_IMAGE_ASPECT_DEPTH_BIT | STENCIL if needed
			&vkContext->imGuiResources.m_ViewportImages[i]
		);
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

// ------------------------------------------------------------------------------------------- //

void NOUS_ImGuiVulkanResources::CreateViewportDescriptorSets(VulkanContext* vkContext)
{
	for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		vkContext->imGuiResources.m_ViewportDescriptorSets[i] = ImGui_ImplVulkan_AddTexture(
			vkContext->imGuiResources.m_ViewportTextureSampler, 
			vkContext->imGuiResources.m_ViewportImages[i].view, 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
}

ImTextureID NOUS_ImGuiVulkanResources::GetViewportTexture(VulkanContext* vkContext, uint32 imageIndex)
{
	VkDescriptorSet currentViewportTexture = vkContext->imGuiResources.m_ViewportDescriptorSets[imageIndex];
	return reinterpret_cast<ImTextureID>(currentViewportTexture);
}

void NOUS_ImGuiVulkanResources::DestroyViewportDescriptorSets(VulkanContext* vkContext)
{
	for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
	{
		ImGui_ImplVulkan_RemoveTexture(vkContext->imGuiResources.m_ViewportDescriptorSets[i]);
	}
}

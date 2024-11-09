#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanFramebuffer 
{
	bool CreateFramebuffers(VulkanContext* vkContext);

	void DestroyFramebuffers(VulkanContext* vkContext);

	// ------------------------------------------------------------------------------------------------------------ //

	void CreateVulkanFramebuffer(VulkanContext* vkContext, VulkanRenderpass* renderpass, uint32 width, uint32 height,
		uint32 attachmentCount, VkImageView* attachments, VulkanFramebuffer* outFramebuffer);

	void DestroyVulkanFramebuffer(VulkanContext* vkContext, VulkanFramebuffer* framebuffer);
}
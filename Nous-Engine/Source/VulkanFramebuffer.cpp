#include "VulkanFramebuffer.h"
#include "VulkanUtils.h"

bool NOUS_VulkanFramebuffer::CreateFramebuffers(VulkanContext* vkContext)
{
    bool ret = true;

	uint32 imageCount = vkContext->swapChain.swapChainFramebuffers.size();

	for (uint16 i = 0; i < imageCount; ++i)
	{
		// World Attachments

		std::array<VkImageView, 2> worldAttachments = { vkContext->imGuiResources.m_ViewportImages[i].view, vkContext->imGuiResources.m_ViewportDepthAttachment.view };

		VkFramebufferCreateInfo worldFramebufferCreateInfo{};
		worldFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		worldFramebufferCreateInfo.renderPass = vkContext->mainRenderpass.handle;
		worldFramebufferCreateInfo.attachmentCount = static_cast<uint32>(worldAttachments.size());
		worldFramebufferCreateInfo.pAttachments = worldAttachments.data();
		worldFramebufferCreateInfo.width = vkContext->framebufferWidth;
		worldFramebufferCreateInfo.height = vkContext->framebufferHeight;
		worldFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &worldFramebufferCreateInfo,
			vkContext->allocator, &vkContext->worldFramebuffers[i]));

		// UI Attachments
		
		std::array<VkImageView, 2> uiAttachments = { vkContext->swapChain.swapChainImageViews[i], vkContext->swapChain.depthAttachment.view};

		VkFramebufferCreateInfo uiFramebufferCreateInfo{};
		uiFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		uiFramebufferCreateInfo.renderPass = vkContext->uiRenderpass.handle;
		uiFramebufferCreateInfo.attachmentCount = static_cast<uint32>(uiAttachments.size());
		uiFramebufferCreateInfo.pAttachments = uiAttachments.data();
		uiFramebufferCreateInfo.width = vkContext->framebufferWidth;
		uiFramebufferCreateInfo.height = vkContext->framebufferHeight;
		uiFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &uiFramebufferCreateInfo,
			vkContext->allocator, &vkContext->swapChain.swapChainFramebuffers[i]));
	}

    return ret;
}

void NOUS_VulkanFramebuffer::DestroyFramebuffers(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Framebuffers...");

    for (uint16 i = 0; i < vkContext->swapChain.swapChainFramebuffers.size(); ++i) 
    {
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->worldFramebuffers[i], vkContext->allocator);
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->swapChain.swapChainFramebuffers[i], vkContext->allocator);
    }
}
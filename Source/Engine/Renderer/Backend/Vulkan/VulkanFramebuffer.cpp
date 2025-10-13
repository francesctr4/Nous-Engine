#include "VulkanFramebuffer.h"
#include "VulkanUtils.h"

bool NOUS_VulkanFramebuffer::CreateFramebuffers(VulkanContext* vkContext)
{
    bool ret = true;

	uint32 imageCount = static_cast<uint32>(vkContext->swapChain.swapChainFramebuffers.size());

	for (uint16 i = 0; i < imageCount; ++i)
	{
		// Scene Viewport Attachments

		std::array<VkImageView, 2> sceneAttachments = { vkContext->imGuiResources.m_ViewportImages[i].view, vkContext->imGuiResources.m_ViewportDepthAttachment.view };

		VkFramebufferCreateInfo sceneFramebufferCreateInfo{};
		sceneFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		sceneFramebufferCreateInfo.renderPass = vkContext->sceneRenderpass.handle;
		sceneFramebufferCreateInfo.attachmentCount = static_cast<uint32>(sceneAttachments.size());
		sceneFramebufferCreateInfo.pAttachments = sceneAttachments.data();
		sceneFramebufferCreateInfo.width = vkContext->framebufferWidth;
		sceneFramebufferCreateInfo.height = vkContext->framebufferHeight;
		sceneFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &sceneFramebufferCreateInfo,
			vkContext->allocator, &vkContext->imGuiResources.m_ViewportFramebuffers[i]));

		// Game Viewport Attachments

		std::array<VkImageView, 2> gameAttachments = { vkContext->imGuiResources.m_GameViewportImages[i].view, vkContext->imGuiResources.m_GameViewportDepthAttachment.view };

		VkFramebufferCreateInfo gameFramebufferCreateInfo{};
		gameFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		gameFramebufferCreateInfo.renderPass = vkContext->gameRenderpass.handle;
		gameFramebufferCreateInfo.attachmentCount = static_cast<uint32>(gameAttachments.size());
		gameFramebufferCreateInfo.pAttachments = gameAttachments.data();
		gameFramebufferCreateInfo.width = vkContext->framebufferWidth;
		gameFramebufferCreateInfo.height = vkContext->framebufferHeight;
		gameFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &gameFramebufferCreateInfo,
			vkContext->allocator, &vkContext->imGuiResources.m_GameViewportFramebuffers[i]));

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
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportFramebuffers[i], vkContext->allocator);
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_GameViewportFramebuffers[i], vkContext->allocator);
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->swapChain.swapChainFramebuffers[i], vkContext->allocator);
    }
}
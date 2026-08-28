#include "VulkanFramebuffer.h"
#include "Utils/VulkanUtils.h"

bool NOUS_VulkanFramebuffer::CreateFramebuffers(VulkanContext* vkContext)
{
    bool ret = true;

	// Derive the count from the actual swapchain image views (runtime count), not from
	// the framebuffer container — the latter is empty on first creation and would skip
	// the loop. Resize the per-image framebuffer vectors to match before filling them.
	uint32_t imageCount = static_cast<uint32_t>(vkContext->swapChain.swapChainImageViews.size());

	if (vkContext->renderMode == RenderMode::GAME)
	{
		vkContext->gameSwapchainFramebuffers.resize(imageCount);

		// GAME mode: create framebuffers that target swapchain image views directly.
		for (uint32_t i = 0; i < imageCount; ++i)
		{
			std::array<VkImageView, 2> attachments = {
				vkContext->swapChain.swapChainImageViews[i],
				vkContext->swapChain.depthAttachment.view
			};

			VkFramebufferCreateInfo fbInfo{};
			fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.renderPass      = vkContext->gameSwapchainRenderpass.handle;
			fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			fbInfo.pAttachments    = attachments.data();
			fbInfo.width           = vkContext->framebufferWidth;
			fbInfo.height          = vkContext->framebufferHeight;
			fbInfo.layers          = 1;

			VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &fbInfo,
				vkContext->allocator, &vkContext->gameSwapchainFramebuffers[i]));
		}
		return ret;
	}

	vkContext->imGuiResources.m_ViewportFramebuffers.resize(imageCount);
	vkContext->imGuiResources.m_GameViewportFramebuffers.resize(imageCount);
	vkContext->swapChain.swapChainFramebuffers.resize(imageCount);

	for (uint16_t i = 0; i < imageCount; ++i)
	{
		// Scene Viewport Attachments

		std::array<VkImageView, 2> sceneAttachments = { vkContext->imGuiResources.m_ViewportImages[i].view, vkContext->imGuiResources.m_ViewportDepthAttachment.view };

		VkFramebufferCreateInfo sceneFramebufferCreateInfo{};
		sceneFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		sceneFramebufferCreateInfo.renderPass = vkContext->sceneRenderpass.handle;
		sceneFramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(sceneAttachments.size());
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
		gameFramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(gameAttachments.size());
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
		uiFramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(uiAttachments.size());
		uiFramebufferCreateInfo.pAttachments = uiAttachments.data();
		uiFramebufferCreateInfo.width = vkContext->framebufferWidth;
		uiFramebufferCreateInfo.height = vkContext->framebufferHeight;
		uiFramebufferCreateInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &uiFramebufferCreateInfo,
			vkContext->allocator, &vkContext->swapChain.swapChainFramebuffers[i]));
	}

	// Pick Framebuffer (single — used for on-demand mouse picking)

	std::array<VkImageView, 2> pickAttachments = { vkContext->imGuiResources.m_PickImage.view, vkContext->imGuiResources.m_PickDepthAttachment.view };

	VkFramebufferCreateInfo pickFramebufferCreateInfo{};
	pickFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	pickFramebufferCreateInfo.renderPass = vkContext->pickRenderpass.handle;
	pickFramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(pickAttachments.size());
	pickFramebufferCreateInfo.pAttachments = pickAttachments.data();
	pickFramebufferCreateInfo.width = vkContext->framebufferWidth;
	pickFramebufferCreateInfo.height = vkContext->framebufferHeight;
	pickFramebufferCreateInfo.layers = 1;

	VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &pickFramebufferCreateInfo,
		vkContext->allocator, &vkContext->imGuiResources.m_PickFramebuffer));

    return ret;
}

void NOUS_VulkanFramebuffer::DestroyFramebuffers(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Framebuffers...");

    if (vkContext->renderMode == RenderMode::GAME)
    {
        for (uint32_t i = 0; i < vkContext->gameSwapchainFramebuffers.size(); ++i)
        {
            if (vkContext->gameSwapchainFramebuffers[i])
            {
                vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->gameSwapchainFramebuffers[i], vkContext->allocator);
                vkContext->gameSwapchainFramebuffers[i] = VK_NULL_HANDLE;
            }
        }
        return;
    }

    if (vkContext->imGuiResources.m_PickFramebuffer)
    {
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_PickFramebuffer, vkContext->allocator);
		vkContext->imGuiResources.m_PickFramebuffer = VK_NULL_HANDLE;
    }

    for (uint16_t i = 0; i < vkContext->swapChain.swapChainFramebuffers.size(); ++i)
    {
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportFramebuffers[i], vkContext->allocator);
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_GameViewportFramebuffers[i], vkContext->allocator);
		vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->swapChain.swapChainFramebuffers[i], vkContext->allocator);
    }
}
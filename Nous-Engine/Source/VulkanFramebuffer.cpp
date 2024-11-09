#include "VulkanFramebuffer.h"

bool NOUS_VulkanFramebuffer::CreateFramebuffers(VulkanContext* vkContext)
{
    bool ret = true;

    vkContext->swapChain.swapChainFramebuffers.resize(vkContext->swapChain.swapChainImageViews.size());

    for (uint16 i = 0; i < vkContext->swapChain.swapChainFramebuffers.size(); ++i)
    {
        // TODO: make this dynamic based on the currently configured attachments
        std::array<VkImageView, 3> attachments = { vkContext->swapChain.colorAttachment.view, vkContext->swapChain.depthAttachment.view, vkContext->swapChain.swapChainImageViews[i] };

        CreateVulkanFramebuffer(vkContext, &vkContext->mainRenderpass, vkContext->framebufferWidth, vkContext->framebufferHeight,
            attachments.size(), attachments.data(), &vkContext->swapChain.swapChainFramebuffers[i]);
    }

    return ret;
}

void NOUS_VulkanFramebuffer::DestroyFramebuffers(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Framebuffers...");

    for (uint16 i = 0; i < vkContext->swapChain.swapChainFramebuffers.size(); ++i) 
    {
        DestroyVulkanFramebuffer(vkContext, &vkContext->swapChain.swapChainFramebuffers[i]);
    }
}

void NOUS_VulkanFramebuffer::CreateVulkanFramebuffer(VulkanContext* vkContext, VulkanRenderpass* renderpass, uint32 width, uint32 height, uint32 attachmentCount, VkImageView* attachments, VulkanFramebuffer* outFramebuffer)
{
    // Take a copy of the attachments, renderpass and attachment count

    outFramebuffer->attachments.resize(attachmentCount);

    for (uint32 i = 0; i < outFramebuffer->attachments.size(); ++i)
    {
        outFramebuffer->attachments[i] = attachments[i];
    }

    outFramebuffer->renderpass = renderpass;

    // Creation info

    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

    framebufferCreateInfo.renderPass = renderpass->handle;

    framebufferCreateInfo.attachmentCount = static_cast<uint32>(outFramebuffer->attachments.size());
    framebufferCreateInfo.pAttachments = outFramebuffer->attachments.data();

    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;

    framebufferCreateInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(vkContext->device.logicalDevice, &framebufferCreateInfo,
        vkContext->allocator, &outFramebuffer->handle));
}

void NOUS_VulkanFramebuffer::DestroyVulkanFramebuffer(VulkanContext* vkContext, VulkanFramebuffer* framebuffer)
{
    vkDestroyFramebuffer(vkContext->device.logicalDevice, framebuffer->handle, vkContext->allocator);

    framebuffer->attachments.clear();
    framebuffer->attachments.shrink_to_fit();

    framebuffer->handle = 0;
    framebuffer->renderpass = nullptr;
}

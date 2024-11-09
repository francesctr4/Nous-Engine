#include "VulkanRenderpass.h"
#include "VulkanDevice.h"

#include "MemoryManager.h"

bool CreateRenderpass(VulkanContext* vkContext, VulkanRenderpass* outRenderpass,
    float32 x, float32 y, float32 w, float32 h, 
    float32 r, float32 g, float32 b, float32 a, 
    float32 depth, uint32 stencil)
{
    bool ret = false;

    // Transfer values to our Renderpass structure

    outRenderpass->x = x;
    outRenderpass->y = y;
    outRenderpass->w = w;
    outRenderpass->h = h;

    outRenderpass->r = r;
    outRenderpass->g = g;
    outRenderpass->b = b;
    outRenderpass->a = a;

    outRenderpass->depth = depth;
    outRenderpass->stencil = stencil;

    // Color 

    VkAttachmentDescription colorAttachment{};

    colorAttachment.format = vkContext->swapChain.swapChainImageFormat;
    colorAttachment.samples = vkContext->device.msaaSamples;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};

    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth

    VkAttachmentDescription depthAttachment{};

    depthAttachment.format = FindDepthFormat(vkContext->device.physicalDevice);
    depthAttachment.samples = vkContext->device.msaaSamples;

    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};

    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color Resolve (Multisampling)

    VkAttachmentDescription colorAttachmentResolve{};

    colorAttachmentResolve.format = vkContext->swapChain.swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef{};

    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // TODO: Color, Depth, Resolve, Input, Preserve Attachments
    VkSubpassDescription subpass{};

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    //TODO:
    //subpass.inputAttachmentCount = 0;
    //subpass.pInputAttachments = 0;
    //subpass.preserveAttachmentCount = 0;
    //subpass.pPreserveAttachments = 0;

    VkSubpassDependency dependency{};

    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependency.dependencyFlags = 0;

    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

    VkRenderPassCreateInfo renderPassInfo{};

    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();

    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(vkContext->device.logicalDevice, &renderPassInfo, vkContext->allocator, &outRenderpass->handle));
    ret = true;

    return ret;
}

void DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass)
{
    NOUS_DEBUG("Destroying Render Pass...");

    if (renderpass && renderpass->handle) 
    {
        vkDestroyRenderPass(vkContext->device.logicalDevice, renderpass->handle, vkContext->allocator);
        renderpass->handle = 0;
    }
}

void BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer)
{
    VkRenderPassBeginInfo renderPassInfo{};

    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderpass->handle;
    renderPassInfo.framebuffer = frameBuffer;

    renderPassInfo.renderArea.offset = { static_cast<int32>(renderpass->x), static_cast<int32>(renderpass->y) };
    renderPassInfo.renderArea.extent = { static_cast<uint32>(renderpass->w), static_cast<uint32>(renderpass->h) };

    std::array<VkClearValue, 2> clearValues{};

    clearValues[0].color = { {renderpass->r, renderpass->g, renderpass->b, renderpass->a} };
    clearValues[1].depthStencil = { renderpass->depth, renderpass->stencil };

    renderPassInfo.clearValueCount = static_cast<uint32>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer->handle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    commandBuffer->state = VulkanCommandBufferState::COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void EndRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass)
{
    vkCmdEndRenderPass(commandBuffer->handle);
    commandBuffer->state = VulkanCommandBufferState::COMMAND_BUFFER_STATE_RECORDING;
}

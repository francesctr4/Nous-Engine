#include "VulkanRenderpass.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include "MemoryManager.h"

bool NOUS_VulkanRenderpass::CreateRenderpass(
    VulkanContext* vkContext,
    VulkanRenderpass* outRenderpass,
    float4 renderArea, float4 clearColor,
    float depth, uint32 stencil, uint8 clearFlags,
    bool prevPass, bool nextPass)
{
    bool ret = false;

    outRenderpass->clearFlags = clearFlags;
    outRenderpass->renderArea = renderArea;
    outRenderpass->clearColor = clearColor;
    outRenderpass->prevPass = prevPass;
    outRenderpass->nextPass = nextPass;

    outRenderpass->depth = depth;
    outRenderpass->stencil = stencil;

    uint32 attachmentCount = 0;
    std::array<VkAttachmentDescription, 3> attachments;

    // --- Color Attachment (Always Present) ---
    bool doClearColor = (outRenderpass->clearFlags & RenderpassClearFlag::COLOR_BUFFER) != 0;
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vkContext->swapChain.swapChainImageFormat;
    colorAttachment.samples = vkContext->device.msaaSamples;
    colorAttachment.loadOp = doClearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Fix: Set initialLayout based on loadOp
    colorAttachment.initialLayout = doClearColor ?
        (prevPass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED) :
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colorAttachment.finalLayout = nextPass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[attachmentCount++] = colorAttachment;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // --- Depth Attachment (Always Present) ---
    bool doClearDepth = (outRenderpass->clearFlags & RenderpassClearFlag::DEPTH_BUFFER) != 0;
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat(vkContext->device.physicalDevice);
    depthAttachment.samples = vkContext->device.msaaSamples;
    depthAttachment.loadOp = doClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Adjust if needed
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Fix: Set initialLayout based on loadOp
    depthAttachment.initialLayout = doClearDepth ?
        VK_IMAGE_LAYOUT_UNDEFINED :
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[attachmentCount++] = depthAttachment;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // --- Resolve Attachment (Always Present) ---
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = vkContext->swapChain.swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[attachmentCount++] = colorAttachmentResolve;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // --- Subpass Setup ---
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef; // Always reference depth
    subpass.pResolveAttachments = &colorAttachmentResolveRef; // Always reference resolve

    // Dependency setup
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Render pass creation
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachmentCount; // Corrected to actual count
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(vkContext->device.logicalDevice, &renderPassInfo, vkContext->allocator, &outRenderpass->handle));
    ret = true;

    return ret;
}

void NOUS_VulkanRenderpass::DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass)
{
    NOUS_DEBUG("Destroying Render Pass...");

    if (renderpass && renderpass->handle) 
    {
        vkDestroyRenderPass(vkContext->device.logicalDevice, renderpass->handle, vkContext->allocator);
        renderpass->handle = 0;
    }
}

void NOUS_VulkanRenderpass::BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer)
{
    VkRenderPassBeginInfo renderPassInfo{};

    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderpass->handle;
    renderPassInfo.framebuffer = frameBuffer;

    renderPassInfo.renderArea.offset = { static_cast<int32>(renderpass->renderArea.x), static_cast<int32>(renderpass->renderArea.y) };
    renderPassInfo.renderArea.extent = { static_cast<uint32>(renderpass->renderArea.z), static_cast<uint32>(renderpass->renderArea.w) };

    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    std::array<VkClearValue, 2> clearValues{};

    bool doClearColor = (renderpass->clearFlags & RenderpassClearFlag::COLOR_BUFFER) != 0;

    if (doClearColor) 
    {
        MemoryManager::CopyMemory(clearValues[renderPassInfo.clearValueCount].color.float32, 
            renderpass->clearColor.ptr(), sizeof(float4));

        renderPassInfo.clearValueCount++;
    }

    bool doClearDepth = (renderpass->clearFlags & RenderpassClearFlag::DEPTH_BUFFER) != 0;

    if (doClearDepth)
    {
        MemoryManager::CopyMemory(clearValues[renderPassInfo.clearValueCount].color.float32,
            renderpass->clearColor.ptr(), sizeof(float4));

        clearValues[renderPassInfo.clearValueCount].depthStencil.depth = renderpass->depth;
        clearValues[renderPassInfo.clearValueCount].depthStencil.stencil = renderpass->stencil;
        bool doClearStencil = (renderpass->clearFlags & RenderpassClearFlag::STENCIL_BUFFER) != 0;

        if (doClearStencil)
        {
            clearValues[renderPassInfo.clearValueCount].depthStencil.stencil = renderpass->stencil;
        }

        renderPassInfo.clearValueCount++;
    }

    renderPassInfo.pClearValues = renderPassInfo.clearValueCount > 0 ? clearValues.data() : nullptr;

    vkCmdBeginRenderPass(commandBuffer->handle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    commandBuffer->state = VulkanCommandBufferState::IN_RENDER_PASS;
}

void NOUS_VulkanRenderpass::EndRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass)
{
    vkCmdEndRenderPass(commandBuffer->handle);
    commandBuffer->state = VulkanCommandBufferState::RECORDING;
}

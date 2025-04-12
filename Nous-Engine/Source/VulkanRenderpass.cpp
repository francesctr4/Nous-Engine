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
    std::array<VkAttachmentDescription, 2> attachments;

    // --- Color Attachment (Always Present) ---
    bool doClearColor = (outRenderpass->clearFlags & RenderpassClearFlag::COLOR_BUFFER) != 0;
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vkContext->swapChain.swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = doClearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Fix: Set initialLayout based on loadOp
    colorAttachment.initialLayout = doClearColor ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[attachmentCount++] = colorAttachment;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // --- Depth Attachment (Always Present) ---
    bool doClearDepth = (outRenderpass->clearFlags & RenderpassClearFlag::DEPTH_BUFFER) != 0;
    // In CreateRenderpass (UI render pass)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat(vkContext->device.physicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = doClearDepth ?
        VK_ATTACHMENT_LOAD_OP_CLEAR :
        VK_ATTACHMENT_LOAD_OP_LOAD; // Preserve depth data if not clearing

    // Match the offscreen pass's final layout when loading
    depthAttachment.initialLayout = doClearDepth ?
        VK_IMAGE_LAYOUT_UNDEFINED : // Clear requires UNDEFINED layout
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Preserve layout

    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[attachmentCount++] = depthAttachment;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // --- Subpass Setup ---
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef; // Always reference depth

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

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

bool NOUS_VulkanRenderpass::CreateOffscreenRenderpass(
    VulkanContext* vkContext,
    VulkanRenderpass* outRenderpass,
    float4 renderArea, float4 clearColor,
    float depth, uint32 stencil, uint8 clearFlags)
{
    outRenderpass->clearFlags = clearFlags;
    outRenderpass->renderArea = renderArea;
    outRenderpass->clearColor = clearColor;

    outRenderpass->depth = depth;
    outRenderpass->stencil = stencil;

    {
        std::array<VkAttachmentDescription, 2> attachments = {};
        // Color attachment
        attachments[0].format = vkContext->swapChain.swapChainImageFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Depth attachment
        attachments[1].format = FindDepthFormat(vkContext->device.physicalDevice);
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference = {};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthReference = {};
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = &depthReference;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;
        subpassDescription.pResolveAttachments = nullptr;

        // Subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> dependencies;

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask =
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | // Wait for previous depth writes
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | // Depth testing/clearing
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Color writes
        dependencies[0].srcAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | // Previous depth writes
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | // Offscreen depth writes
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescription;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(vkContext->device.logicalDevice, &renderPassInfo, vkContext->allocator, &outRenderpass->handle) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    return true;
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

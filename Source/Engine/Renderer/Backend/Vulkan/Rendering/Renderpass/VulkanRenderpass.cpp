#include "VulkanRenderpass.h"
#include "Engine/Renderer/Backend/Vulkan/Core/Device/VulkanDevice.h"
#include "Engine/Renderer/Backend/Vulkan/Utils/VulkanUtils.h"

#include <MemoryManager/MemoryManager.h>

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

bool NOUS_VulkanRenderpass::CreateRenderpass(
    VulkanContext* vkContext,
    VulkanRenderpass* outRenderpass,
    glm::vec4 renderArea, glm::vec4 clearColor,
    float depth, uint32 stencil, uint8 clearFlags,
    bool prevPass, bool nextPass,
    bool offscreen,
    VkFormat colorFormatOverride)
{
    outRenderpass->clearFlags = clearFlags;
    outRenderpass->renderArea = renderArea;
    outRenderpass->clearColor = clearColor;
    outRenderpass->prevPass   = prevPass;
    outRenderpass->nextPass   = nextPass;
    outRenderpass->depth      = depth;
    outRenderpass->stencil    = stencil;

    const bool doClearColor = (clearFlags & RenderpassClearFlag::COLOR_BUFFER) != 0;
    const bool doClearDepth = (clearFlags & RenderpassClearFlag::DEPTH_BUFFER) != 0;

    // ── Attachments ───────────────────────────────────────────────────────────

    std::array<VkAttachmentDescription, 2> attachments{};

    // Color attachment
    attachments[0].format         = (colorFormatOverride != VK_FORMAT_UNDEFINED)
                                    ? colorFormatOverride
                                    : vkContext->swapChain.swapChainImageFormat;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = doClearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // Offscreen passes always start from UNDEFINED (fresh texture each frame).
    // Present passes start from UNDEFINED when clearing, otherwise preserve the current swapchain layout.
    attachments[0].initialLayout  = (offscreen || doClearColor) ? VK_IMAGE_LAYOUT_UNDEFINED
                                                                 : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Offscreen: transition to SHADER_READ_ONLY so the output can be sampled in the next pass.
    // Present:   transition to PRESENT_SRC_KHR for swapchain presentation.
    attachments[0].finalLayout    = offscreen ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                              : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format         = NOUS_VulkanDevice::FindDepthFormat(vkContext->device.physicalDevice);
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = doClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    // Offscreen passes store depth so it remains available for subsequent passes.
    attachments[1].storeOp        = offscreen ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp  = doClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = doClearDepth ? VK_IMAGE_LAYOUT_UNDEFINED
                                                 : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // ── Subpass ───────────────────────────────────────────────────────────────

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // ── Subpass dependencies ──────────────────────────────────────────────────
    // Both pass types share the first dependency (external → subpass 0).
    // Offscreen passes add a second (subpass 0 → external) to ensure the color
    // output is fully written and transitioned before it is sampled as a texture.

    std::array<VkSubpassDependency, 2> dependencies{};
    uint32_t dependencyCount = 1;

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    if (offscreen)
    {
        dependencies[1].srcSubpass      = 0;
        dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencyCount = 2;
    }

    // ── Render pass creation ──────────────────────────────────────────────────

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = dependencyCount;
    renderPassInfo.pDependencies   = dependencies.data();

    VK_CHECK(vkCreateRenderPass(vkContext->device.logicalDevice, &renderPassInfo,
        vkContext->allocator, &outRenderpass->handle));

    return true;
}

void NOUS_VulkanRenderpass::DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass)
{
    if (renderpass && renderpass->handle)
    {
        vkDestroyRenderPass(vkContext->device.logicalDevice, renderpass->handle, vkContext->allocator);
        renderpass->handle = 0;
    }
}

void NOUS_VulkanRenderpass::BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass  = renderpass->handle;
    renderPassInfo.framebuffer = frameBuffer;

    renderPassInfo.renderArea.offset = { static_cast<int32>(renderpass->renderArea.x),
                                         static_cast<int32>(renderpass->renderArea.y) };
    renderPassInfo.renderArea.extent = { static_cast<uint32>(renderpass->renderArea.z),
                                         static_cast<uint32>(renderpass->renderArea.w) };

    std::array<VkClearValue, 2> clearValues{};
    renderPassInfo.clearValueCount = 0;

    if (renderpass->clearFlags & RenderpassClearFlag::COLOR_BUFFER)
    {
        nous::engine::memory::CopyMemory(clearValues[renderPassInfo.clearValueCount].color.float32,
                                  glm::value_ptr(renderpass->clearColor), sizeof(glm::vec4));
        renderPassInfo.clearValueCount++;
    }

    if (renderpass->clearFlags & RenderpassClearFlag::DEPTH_BUFFER)
    {
        nous::engine::memory::CopyMemory(clearValues[renderPassInfo.clearValueCount].color.float32,
                                  glm::value_ptr(renderpass->clearColor), sizeof(glm::vec4));
        clearValues[renderPassInfo.clearValueCount].depthStencil.depth   = renderpass->depth;
        clearValues[renderPassInfo.clearValueCount].depthStencil.stencil = renderpass->stencil;

        if (renderpass->clearFlags & RenderpassClearFlag::STENCIL_BUFFER)
            clearValues[renderPassInfo.clearValueCount].depthStencil.stencil = renderpass->stencil;

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

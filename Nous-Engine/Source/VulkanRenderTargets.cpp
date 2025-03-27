#include "VulkanRenderTargets.h"
#include "VulkanImage.h"
#include "VulkanRenderpass.h"
#include "VulkanFramebuffer.h"
#include "VulkanUtils.h"

bool NOUS_VulkanRenderTargets::CreateOffscreenRenderTarget(VulkanContext* vkContext) 
{
    // 1. Create offscreen image
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    CreateVulkanImage(
        vkContext,
        VK_IMAGE_TYPE_2D,
        vkContext->framebufferWidth,
        vkContext->framebufferHeight,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_COLOR_BIT,
        &vkContext->renderTarget.offscreenImage
    );

    // 2. Create render pass
    VulkanRenderpass* renderpass = &vkContext->renderTarget.offscreenRenderpass;
    if (!CreateRenderpass(vkContext, renderpass,
        0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight,
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f,
        0)) 
    { // Final layout for sampling
        NOUS_ERROR("Failed to create offscreen render pass");
        return false;
    }

    // 3. Create framebuffer
    VulkanFramebuffer* fb = &vkContext->renderTarget.offscreenFramebuffer;
    fb->attachments.push_back(vkContext->renderTarget.offscreenImage.view);

    NOUS_VulkanFramebuffer::CreateVulkanFramebuffer(
        vkContext,
        renderpass,
        vkContext->framebufferWidth,
        vkContext->framebufferHeight,
        1,
        fb->attachments.data(), 
        fb);

    // 4. Create sampler
    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkResult result = vkCreateSampler(
        vkContext->device.logicalDevice,
        &samplerInfo,
        vkContext->allocator,
        &vkContext->renderTarget.offscreenSampler
    );
    if (!VkResultIsSuccess(result)) {
        NOUS_ERROR("Failed to create offscreen sampler: %s", VkResultMessage(result, true));
        return false;
    }

    if (vkContext->imGuiResources.descriptorPool == VK_NULL_HANDLE) {
        // Create descriptor pool
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
        };

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1;

        VkResult result = vkCreateDescriptorPool(
            vkContext->device.logicalDevice,
            &poolInfo,
            vkContext->allocator,
            &vkContext->imGuiResources.descriptorPool
        );

        if (!VkResultIsSuccess(result)) {
            NOUS_ERROR("Failed to create ImGui descriptor pool: %s", VkResultMessage(result, true));
            return false;
        }
    }

    if (vkContext->imGuiResources.textureDescriptorLayout == VK_NULL_HANDLE) {
        // Create descriptor set layout
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = 0;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layoutBinding;

        VkResult result = vkCreateDescriptorSetLayout(
            vkContext->device.logicalDevice,
            &layoutInfo,
            vkContext->allocator,
            &vkContext->imGuiResources.textureDescriptorLayout
        );

        if (!VkResultIsSuccess(result)) {
            NOUS_ERROR("Failed to create ImGui descriptor set layout: %s", VkResultMessage(result, true));
            return false;
        }
    }

    // 6. Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = vkContext->imGuiResources.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vkContext->imGuiResources.textureDescriptorLayout;

    result = vkAllocateDescriptorSets(
        vkContext->device.logicalDevice,
        &allocInfo,
        &vkContext->renderTarget.offscreenDescriptorSet
    );
    if (!VkResultIsSuccess(result)) {
        NOUS_ERROR("Failed to allocate offscreen descriptor set: %s", VkResultMessage(result, true));
        return false;
    }

    // Update descriptor set
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = vkContext->renderTarget.offscreenImage.view;
    imageInfo.sampler = vkContext->renderTarget.offscreenSampler;

    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet = vkContext->renderTarget.offscreenDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
        vkContext->device.logicalDevice,
        1, &descriptorWrite,
        0, nullptr
    );

    return true;
}

void NOUS_VulkanRenderTargets::BindOffscreenRenderTarget(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer)
{
    BeginRenderpass(commandBuffer, &vkContext->renderTarget.offscreenRenderpass,
        vkContext->renderTarget.offscreenFramebuffer.handle);

    // Set dynamic state for offscreen
    VkViewport viewport = {
        0.0f, 0.0f,
        (float)vkContext->framebufferWidth, (float)vkContext->framebufferHeight,
        0.0f, 1.0f
    };
    vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);

    VkRect2D scissor = { {0, 0}, {(uint32)vkContext->framebufferWidth, (uint32)vkContext->framebufferHeight} };
    vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissor);
}

void NOUS_VulkanRenderTargets::UnbindOffscreenRenderTarget(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer)
{
    EndRenderpass(commandBuffer, &vkContext->renderTarget.offscreenRenderpass);

    // Insert barrier to transition image layout if needed
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.image = vkContext->renderTarget.offscreenImage.handle;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        commandBuffer->handle,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

bool NOUS_VulkanRenderTargets::RecreateOffscreenRenderTarget(VulkanContext* vkContext)
{
    // Before recreating swapchain
    NOUS_VulkanRenderTargets::DestroyOffscreenRenderTarget(vkContext);

    // After recreating swapchain
    if (!NOUS_VulkanRenderTargets::CreateOffscreenRenderTarget(vkContext)) {
        NOUS_ERROR("Failed to recreate offscreen render target!");
        return false;
    }
}

void NOUS_VulkanRenderTargets::DestroyOffscreenRenderTarget(VulkanContext* vkContext)
{
    if (vkContext->imGuiResources.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vkContext->device.logicalDevice,
            vkContext->imGuiResources.descriptorPool,
            vkContext->allocator);
        vkContext->imGuiResources.descriptorPool = VK_NULL_HANDLE;
    }

    if (vkContext->imGuiResources.textureDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vkContext->device.logicalDevice,
            vkContext->imGuiResources.textureDescriptorLayout,
            vkContext->allocator);
        vkContext->imGuiResources.textureDescriptorLayout = VK_NULL_HANDLE;
    }

    // Destroy in reverse order of creation
    vkDestroySampler(vkContext->device.logicalDevice, vkContext->renderTarget.offscreenSampler, vkContext->allocator);
    NOUS_VulkanFramebuffer::DestroyVulkanFramebuffer(vkContext, &vkContext->renderTarget.offscreenFramebuffer);
    DestroyRenderpass(vkContext, &vkContext->renderTarget.offscreenRenderpass);
    DestroyVulkanImage(vkContext, &vkContext->renderTarget.offscreenImage);
}

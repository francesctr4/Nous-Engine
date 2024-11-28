#include "VulkanObjectShader.h"
#include "VulkanShaderUtils.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanExternal.h"
#include "VulkanUtils.h"
#include "VulkanBuffer.h"

#include "Logger.h"
#include "MemoryManager.h"

constexpr const char* BUILTIN_OBJECT_SHADER_NAME = "BuiltIn.ObjectShader";

bool CreateObjectShader(VulkanContext* vkContext, VulkanObjectShader* outShader)
{
#ifdef _DEBUG
    ExecuteBatchFile("compile-shaders.bat");
#endif // _DEBUG

    bool ret = true;

    // Shader module init per stage.
    std::array<std::string, OBJECT_SHADER_STAGE_COUNT> stageTypeStrings = { "vert", "frag" };
    std::array<VkShaderStageFlagBits, OBJECT_SHADER_STAGE_COUNT> stageTypes = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

    for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) 
    {
        if (!CreateShaderModule(vkContext, BUILTIN_OBJECT_SHADER_NAME, stageTypeStrings[i], stageTypes[i], i, outShader->stages.data()))
        {
            NOUS_ERROR("Unable to create %s shader module for '%s'.", stageTypeStrings[i].c_str(), BUILTIN_OBJECT_SHADER_NAME);
            ret = false;
        }
    }

    // Global Descriptors
    VkDescriptorSetLayoutBinding globalUboLayoutBinding;

    globalUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    globalUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    globalUboLayoutBinding.binding = 0;
    globalUboLayoutBinding.descriptorCount = 1;
    globalUboLayoutBinding.pImmutableSamplers = 0;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &globalUboLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(vkContext->device.logicalDevice, &descriptorSetLayoutCreateInfo, vkContext->allocator, &outShader->globalDescriptorSetLayout));

    // Global descriptor pool: Used for global items such as view/projection matrix.
    VkDescriptorPoolSize globalDescriptorPoolSize;
    globalDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    globalDescriptorPoolSize.descriptorCount = vkContext->swapChain.swapChainImages.size();

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &globalDescriptorPoolSize;

    descriptorPoolCreateInfo.maxSets = vkContext->swapChain.swapChainImages.size();

    VK_CHECK(vkCreateDescriptorPool(vkContext->device.logicalDevice, &descriptorPoolCreateInfo, vkContext->allocator, &outShader->globalDescriptorPool));

    // Pipeline creation
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (float)vkContext->framebufferHeight;
    viewport.width = (float)vkContext->framebufferWidth;
    viewport.height = -(float)vkContext->framebufferHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = vkContext->framebufferWidth;
    scissor.extent.height = vkContext->framebufferHeight;

    // Attributes
    VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
    std::array<VkVertexInputAttributeDescription, Vertex::ATTRIBUTE_COUNT> attributeDescriptions = Vertex::GetAttributeDescriptions();

    // Desciptor set layouts
    const int32 DESCRIPTOR_SET_LAYOUT_COUNT = 1;
    std::array<VkDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_COUNT> descriptorSetlayouts = { outShader->globalDescriptorSetLayout };
    
    // Stages
    // NOTE: Should match the number of shader->stages.
    std::array<VkPipelineShaderStageCreateInfo, OBJECT_SHADER_STAGE_COUNT> shaderStageCreateInfos;
    MemoryManager::ZeroMemory(shaderStageCreateInfos.data(), sizeof(shaderStageCreateInfos));

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) 
    {
        shaderStageCreateInfos[i].sType = outShader->stages[i].shaderStageCreateInfo.sType;
        shaderStageCreateInfos[i] = outShader->stages[i].shaderStageCreateInfo;
    }

    if (!CreateGraphicsPipeline(vkContext, &vkContext->mainRenderpass, bindingDescription, 
        static_cast<uint32>(attributeDescriptions.size()), attributeDescriptions.data(),
        static_cast<uint32>(descriptorSetlayouts.size()), descriptorSetlayouts.data(), 
        OBJECT_SHADER_STAGE_COUNT, shaderStageCreateInfos.data(), viewport, scissor,
        false, &outShader->pipeline))
    {
        NOUS_ERROR("Failed to load graphics pipeline for object shader.");

        ret = false;
    }

    // Create uniform buffer.
    if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, sizeof(GlobalUniformObject) * 3,
        VkBufferUsageFlagBits(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true, &outShader->globalUniformBuffer)) 
    {
        NOUS_ERROR("Vulkan Buffer Creation failed for object shader.");
        return false;
    }

    // Allocate global descriptor sets.
    const int32 GLOBAL_DESCRIPTOR_SET_LAYOUTS_COUNT = 3;
    std::array<VkDescriptorSetLayout, GLOBAL_DESCRIPTOR_SET_LAYOUTS_COUNT> globalDescriptorSetLayouts =
    {
        outShader->globalDescriptorSetLayout,
        outShader->globalDescriptorSetLayout,
        outShader->globalDescriptorSetLayout
    };

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    descriptorSetAllocateInfo.descriptorPool = outShader->globalDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32>(globalDescriptorSetLayouts.size());
    descriptorSetAllocateInfo.pSetLayouts = globalDescriptorSetLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(vkContext->device.logicalDevice, &descriptorSetAllocateInfo, outShader->globalDescriptorSets.data()));

    return ret;
}

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    VkDevice logicalDevice = vkContext->device.logicalDevice;

    // Destroy uniform buffer.
    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &shader->globalUniformBuffer);

    // Destroy pipeline.
    DestroyGraphicsPipeline(vkContext, &shader->pipeline);

    // Destroy global descriptor pool.
    vkDestroyDescriptorPool(logicalDevice, shader->globalDescriptorPool, vkContext->allocator);

    // Destroy descriptor set layouts.
    vkDestroyDescriptorSetLayout(logicalDevice, shader->globalDescriptorSetLayout, vkContext->allocator);

    NOUS_DEBUG("Destroying Shader Modules...");
    // Destroy shader modules.
    for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) 
    {
        vkDestroyShaderModule(logicalDevice, shader->stages[i].handle, vkContext->allocator);
        shader->stages[i].handle = 0;
    }
}

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    BindGraphicsPipeline(&vkContext->graphicsCommandBuffers[vkContext->imageIndex],
        VK_PIPELINE_BIND_POINT_GRAPHICS, &shader->pipeline);
}

void UpdateGlobalStateObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    u32 imageIndex = vkContext->imageIndex;
    VkCommandBuffer commandBuffer = vkContext->graphicsCommandBuffers[imageIndex].handle;
    VkDescriptorSet globalDescriptorSet = shader->globalDescriptorSets[imageIndex];

    // Configure the descriptors for the given index.
    u32 range = sizeof(GlobalUniformObject);
    u64 offset = sizeof(GlobalUniformObject) * imageIndex;

    // Copy data to buffer
    NOUS_VulkanBuffer::LoadBuffer(vkContext, &shader->globalUniformBuffer, offset, range, 0, &shader->globalUBO);

    VkDescriptorBufferInfo descriptorBufferInfo;
    descriptorBufferInfo.buffer = shader->globalUniformBuffer.handle;
    descriptorBufferInfo.offset = offset;
    descriptorBufferInfo.range = range;

    // Update descriptor sets.
    VkWriteDescriptorSet writeDescriptorSetInfo{};
    writeDescriptorSetInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    writeDescriptorSetInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSetInfo.descriptorCount = 1;

    writeDescriptorSetInfo.dstSet = shader->globalDescriptorSets[imageIndex];
    writeDescriptorSetInfo.dstBinding = 0;
    writeDescriptorSetInfo.dstArrayElement = 0;

    writeDescriptorSetInfo.pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(vkContext->device.logicalDevice, 1, &writeDescriptorSetInfo, 0, 0);

    // Bind the global descriptor set to be updated.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline.pipelineLayout, 0, 1, &globalDescriptorSet, 0, 0);
}

void UpdateObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader, float4x4 model)
{
    uint32 imageIndex = vkContext->imageIndex;

    VkCommandBuffer commandBuffer = vkContext->graphicsCommandBuffers[imageIndex].handle;

    vkCmdPushConstants(commandBuffer, shader->pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4x4), &model);
}
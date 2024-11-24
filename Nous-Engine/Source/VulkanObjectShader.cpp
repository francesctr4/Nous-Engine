#include "VulkanObjectShader.h"
#include "VulkanShaderUtils.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanPlatform.h"

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

    // TODO: Descriptors

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

    // TODO: Desciptor set layouts.
    
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
        0, 0, OBJECT_SHADER_STAGE_COUNT, shaderStageCreateInfos.data(), viewport, scissor, 
        false, &outShader->pipeline))
    {
        NOUS_ERROR("Failed to load graphics pipeline for object shader.");

        ret = false;
    }

    return ret;
}

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    DestroyGraphicsPipeline(vkContext, &shader->pipeline);

    NOUS_DEBUG("Destroying Shader Modules...");
    // Destroy shader modules.
    for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) 
    {
        vkDestroyShaderModule(vkContext->device.logicalDevice, shader->stages[i].handle, vkContext->allocator);
        shader->stages[i].handle = 0;
    }
}

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    BindGraphicsPipeline(&vkContext->graphicsCommandBuffers[vkContext->imageIndex],
        VK_PIPELINE_BIND_POINT_GRAPHICS, &shader->pipeline);
}
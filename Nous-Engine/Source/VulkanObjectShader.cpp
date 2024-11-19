#include "VulkanObjectShader.h"
#include "VulkanShaderUtils.h"

#include "Logger.h"

constexpr const char* BUILTIN_OBJECT_SHADER_NAME = "BuiltIn.ObjectShader";

bool CreateObjectShader(VulkanContext* vkContext, VulkanObjectShader* outShader)
{
    bool ret = true;

    // Shader module init per stage.
    std::array<std::string, OBJECT_SHADER_STAGE_COUNT> stageTypeStrings = { "vert", "frag" };
    std::array<VkShaderStageFlagBits, OBJECT_SHADER_STAGE_COUNT> stageTypes = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

    for (uint32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) 
    {
        if (!CreateShaderModule(vkContext, BUILTIN_OBJECT_SHADER_NAME, stageTypeStrings[i], stageTypes[i], i, outShader->stages.data()))
        {
            NOUS_ERROR("Unable to create %s shader module for '%s'.", stageTypeStrings[i], BUILTIN_OBJECT_SHADER_NAME);
            ret = false;
        }
    }

    // Descriptors

    return ret;
}

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{

}

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{

}
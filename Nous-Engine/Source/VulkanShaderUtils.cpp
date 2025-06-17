#include "VulkanShaderUtils.h"
#include "VulkanUtils.h"

#include <format>
#include "MemoryManager.h"
#include "FileHandle.h"

bool NOUS_VulkanShaderUtils::CreateShaderModule(VulkanContext* vkContext, std::string name, std::string typeStr, VkShaderStageFlagBits shaderStageFlag, uint32 stageIndex, VulkanShaderStage* shaderStages)
{
    // Build filename.
    std::string filename = std::format("Library/Shaders/{}.{}.spv", name, typeStr);

    MemoryManager::ZeroMemory(&shaderStages[stageIndex].shaderModuleCreateInfo, sizeof(VkShaderModuleCreateInfo));
    shaderStages[stageIndex].shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    // Obtain file handle.
    FileHandle handle;

    if (!handle.Open(filename, FileMode::READ, true)) 
    {
        NOUS_ERROR("Unable to read shader module: %s.", filename.c_str());
        return false;
    }

    // Read the entire file as binary.
    uint64 size = 0;
    char* fileBuffer = nullptr;

    if (!handle.ReadAllBytes(&fileBuffer, &size) || size == 0)
    {
        NOUS_ERROR("Unable to binary read shader module: %s.", filename.c_str());
        handle.Close();

        return false;
    }

    shaderStages[stageIndex].shaderModuleCreateInfo.codeSize = size;
    shaderStages[stageIndex].shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32*>(fileBuffer);

    // Close the file.
    handle.Close();

    VK_CHECK(vkCreateShaderModule(vkContext->device.logicalDevice, &shaderStages[stageIndex].shaderModuleCreateInfo,
        vkContext->allocator, &shaderStages[stageIndex].handle));

    // Shader stage info
    MemoryManager::ZeroMemory(&shaderStages[stageIndex].shaderStageCreateInfo, sizeof(VkPipelineShaderStageCreateInfo));
    shaderStages[stageIndex].shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    shaderStages[stageIndex].shaderStageCreateInfo.stage = shaderStageFlag;
    shaderStages[stageIndex].shaderStageCreateInfo.module = shaderStages[stageIndex].handle;
    shaderStages[stageIndex].shaderStageCreateInfo.pName = "main";

    if (fileBuffer) 
    {
        NOUS_DELETE_ARRAY(fileBuffer, size, MemoryManager::MemoryTag::FILE);
    }

    return true;
}

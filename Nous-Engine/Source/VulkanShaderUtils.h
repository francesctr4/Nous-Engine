#pragma once

#include "VulkanTypes.inl"

bool CreateShaderModule(VulkanContext* vkContext, std::string name, std::string typeStr,
    VkShaderStageFlagBits shaderStageFlag, uint32 stageIndex, VulkanShaderStage* shaderStages);
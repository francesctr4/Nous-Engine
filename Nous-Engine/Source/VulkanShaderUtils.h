#pragma once

#include "VulkanTypes.inl"

#include "MathUtils.h"

static VkVertexInputBindingDescription GetVertexBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};

    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

static std::array<VkVertexInputAttributeDescription, Vertex::ATTRIBUTE_COUNT> GetVertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, Vertex::ATTRIBUTE_COUNT> attributeDescriptions{};

    // Position

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    // Color

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    // Texture Coordinates

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

bool CreateShaderModule(VulkanContext* vkContext, std::string name, std::string typeStr,
    VkShaderStageFlagBits shaderStageFlag, uint32 stageIndex, VulkanShaderStage* shaderStages);
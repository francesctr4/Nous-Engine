#pragma once

#include "VulkanTypes.inl"

#include "MathGeoLib/include/Math/float4x4.h"
#include "MathGeoLib/include/Math/float3.h"
#include "MathGeoLib/include/Math/float2.h"

// --------------- Uniform Buffer Object Struct (UBO) --------------- //

struct UniformBufferObject
{
    alignas(16) float4x4 model;
    alignas(16) float4x4 view;
    alignas(16) float4x4 projection;
};

// --------------- Vertex Struct --------------- //

struct Vertex
{
    float3 position;
    float3 color;
    float2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static const uint16 ATTRIBUTE_COUNT = 3;
    static std::array<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

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

    bool operator==(const Vertex& other) const
    {
        return (position.Equals(other.position) && color.Equals(other.color) && texCoord.Equals(other.texCoord));
    }

};

bool CreateShaderModule(VulkanContext* vkContext, std::string name, std::string typeStr,
    VkShaderStageFlagBits shaderStageFlag, uint32 stageIndex, VulkanShaderStage* shaderStages);
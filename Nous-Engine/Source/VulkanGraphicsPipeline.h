#pragma once

#include "VulkanTypes.inl"

bool CreateGraphicsPipeline(VulkanContext* vkContext, VulkanRenderpass* renderpass,
    VkVertexInputBindingDescription bindingDescription, uint32 attributeDescriptionCount,
    VkVertexInputAttributeDescription* attributeDescriptions, uint32 descriptorSetLayoutCount, 
    VkDescriptorSetLayout* descriptorSetLayouts, uint32 shaderStageCount, VkPipelineShaderStageCreateInfo* shaderStages,
    VkViewport viewport, VkRect2D scissor, bool isWireframe, bool depthTestEnabled, VulkanPipeline* outPipeline);

void DestroyGraphicsPipeline(VulkanContext* vkContext, VulkanPipeline* pipeline);

void BindGraphicsPipeline(VulkanCommandBuffer* commandBuffer, 
    VkPipelineBindPoint bindPoint, VulkanPipeline* pipeline);
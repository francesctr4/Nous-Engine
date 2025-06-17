#include "VulkanGraphicsPipeline.h"
#include "VulkanUtils.h"

#include "MemoryManager.h"
#include "Logger.h"

bool NOUS_VulkanGraphicsPipeline::CreateGraphicsPipeline(VulkanContext* vkContext, VulkanRenderpass* renderpass,
    VkVertexInputBindingDescription bindingDescription, uint32 attributeDescriptionCount,
    VkVertexInputAttributeDescription* attributeDescriptions, uint32 descriptorSetLayoutCount,
    VkDescriptorSetLayout* descriptorSetLayouts, uint32 shaderStageCount, VkPipelineShaderStageCreateInfo* shaderStages,
    VkViewport viewport, VkRect2D scissor, bool isWireframe, bool depthTestEnabled, VulkanPipeline* outPipeline)
{
    // Viewport state
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;

    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

    rasterizationStateCreateInfo.polygonMode = isWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};

    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // Match render pass

    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE; // Disabled for non-MSAA
    multisampleStateCreateInfo.minSampleShading = 1.0f;

    multisampleStateCreateInfo.pSampleMask = nullptr;

    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
    if (depthTestEnabled) 
    {
        depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
        depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;

        depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;

        depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateCreateInfo.minDepthBounds = 0.0f;
        depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

        depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
        depthStencilStateCreateInfo.front = {};
        depthStencilStateCreateInfo.back = {};
    }

    // Color Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    MemoryManager::ZeroMemory(&colorBlendAttachmentState, sizeof(VkPipelineColorBlendAttachmentState));

    colorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachmentState.blendEnable = VK_TRUE;

    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;

    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;

    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    // Dynamic state
    const uint32 DYNAMIC_STATE_COUNT = 3;
    std::array<VkDynamicState, DYNAMIC_STATE_COUNT> dynamicStates = 
    { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    // Vertex input attributes
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;

    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributeDescriptionCount;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    // Push constants
    VkPushConstantRange pushConstant;
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = sizeof(float4x4) * 0;
    pushConstant.size = sizeof(float4x4) * 2;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayoutCount;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;

    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

    // Create the pipeline layout.
    VK_CHECK(vkCreatePipelineLayout(vkContext->device.logicalDevice, &pipelineLayoutCreateInfo,
        vkContext->allocator, &outPipeline->pipelineLayout));

    // Pipeline create
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipelineCreateInfo.stageCount = shaderStageCount;
    pipelineCreateInfo.pStages = shaderStages;

    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = depthTestEnabled ? &depthStencilStateCreateInfo : nullptr;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineCreateInfo.pTessellationState = 0;

    pipelineCreateInfo.layout = outPipeline->pipelineLayout;

    pipelineCreateInfo.renderPass = renderpass->handle;
    pipelineCreateInfo.subpass = 0;

    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(vkContext->device.logicalDevice, VK_NULL_HANDLE, 1,
        &pipelineCreateInfo, vkContext->allocator, &outPipeline->handle);

    if (VkResultIsSuccess(result)) 
    {
        NOUS_DEBUG("Vulkan Graphics pipeline created successfully!");
        return true;
    }

    NOUS_ERROR("vkCreateGraphicsPipelines failed with %s.", VkResultMessage(result, true).c_str());

    return false;
}

void NOUS_VulkanGraphicsPipeline::DestroyGraphicsPipeline(VulkanContext* vkContext, VulkanPipeline* pipeline)
{
    NOUS_DEBUG("Destroying Graphics Pipeline...");
    if (pipeline) 
    {
        // Destroy pipeline
        if (pipeline->handle) 
        {
            vkDestroyPipeline(vkContext->device.logicalDevice, pipeline->handle, vkContext->allocator);
            pipeline->handle = 0;
        }

        // Destroy layout
        if (pipeline->pipelineLayout) 
        {
            vkDestroyPipelineLayout(vkContext->device.logicalDevice, pipeline->pipelineLayout, vkContext->allocator);
            pipeline->pipelineLayout = 0;
        }
    }
}

void NOUS_VulkanGraphicsPipeline::BindGraphicsPipeline(VulkanCommandBuffer* commandBuffer,
	VkPipelineBindPoint bindPoint, VulkanPipeline* pipeline)
{
	vkCmdBindPipeline(commandBuffer->handle, bindPoint, pipeline->handle);
}
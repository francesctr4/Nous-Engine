#include "VulkanShader.h"

#include "Engine/Renderer/Backend/Vulkan/Shaders/VulkanShaderUtils.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/GraphicsPipeline/VulkanGraphicsPipeline.h"
#include "Engine/Renderer/Backend/Vulkan/Utils/VulkanUtils.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionTypes.h"

#include <algorithm>

// ─────────────────────────────── Helpers ─────────────────────────────────────

static VkShaderStageFlagBits ToVkStageFlagBit(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:         return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::TessControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Geometry:       return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Fragment:       return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute:        return VK_SHADER_STAGE_COMPUTE_BIT;
        default:                          return VK_SHADER_STAGE_ALL;
    }
}

// Converts the engine's custom stage bitmask (ShaderReflection.cpp ToStageMask) to
// VkShaderStageFlags. Mapping: Vertex=bit0, TessCtrl=bit1, TessEval=bit2,
// Geometry=bit3, Fragment=bit4, Compute=bit5.
static VkShaderStageFlags StageMaskToVkFlags(uint32_t mask)
{
    VkShaderStageFlags flags = 0;
    if (mask & (1u << 0)) flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (mask & (1u << 1)) flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (mask & (1u << 2)) flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (mask & (1u << 3)) flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (mask & (1u << 4)) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (mask & (1u << 5)) flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

static VkDescriptorType ToVkDescriptorType(DescriptorType dt)
{
    switch (dt)
    {
        case DescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::SampledImage:         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::Sampler:              return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default:                                   return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

static VkFormat DataTypeToVkFormat(DataType dt)
{
    switch (dt)
    {
        case DataType::Float: return VK_FORMAT_R32_SFLOAT;
        case DataType::Vec2:  return VK_FORMAT_R32G32_SFLOAT;
        case DataType::Vec3:  return VK_FORMAT_R32G32B32_SFLOAT;
        case DataType::Vec4:  return VK_FORMAT_R32G32B32A32_SFLOAT;
        case DataType::Int:   return VK_FORMAT_R32_SINT;
        case DataType::IVec2: return VK_FORMAT_R32G32_SINT;
        case DataType::IVec3: return VK_FORMAT_R32G32B32_SINT;
        case DataType::IVec4: return VK_FORMAT_R32G32B32A32_SINT;
        case DataType::UInt:  return VK_FORMAT_R32_UINT;
        case DataType::UVec2: return VK_FORMAT_R32G32_UINT;
        case DataType::UVec3: return VK_FORMAT_R32G32B32_UINT;
        case DataType::UVec4: return VK_FORMAT_R32G32B32A32_UINT;
        default:              return VK_FORMAT_UNDEFINED;
    }
}

// ─────────────────────────────── IBackendShader ───────────────────────────────

void VulkanShader::Bind()
{
    // Pipeline binding is issued per-draw-call via the command buffer.
}

void VulkanShader::Destroy()
{
    if (vkContext)
        NOUS_VulkanShader::Destroy(vkContext, this);
}

// ─────────────────────────────── Create ──────────────────────────────────────

bool NOUS_VulkanShader::Create(VulkanContext* vkContext, VulkanRenderpass* renderpass,
    ResourceShader* shader)
{
    if (!shader || shader->stagesData.empty())
    {
        NOUS_ERROR("[VulkanShader] Cannot create shader: no stage data.");
        return false;
    }

    VulkanShader* vs = NOUS_NEW<VulkanShader>(MemoryTag::RENDERER);
    vs->vkContext = vkContext;

    const PipelineReflectionResult& refl = shader->reflection;
    VkDevice dev = vkContext->device.logicalDevice;

    // ── 1. Shader modules ─────────────────────────────────────────────────────
    vs->stages.resize(shader->stagesData.size());

    for (size_t i = 0; i < shader->stagesData.size(); ++i)
    {
        const ShaderSource& src = shader->stagesData[i];

        if (!NOUS_VulkanShaderUtils::CreateShaderModuleFromBinary(
                vkContext, src.spirvBinary, ToVkStageFlagBit(src.stage), &vs->stages[i]))
        {
            NOUS_ERROR("[VulkanShader] Failed to create shader module for stage %d.", (int)src.stage);
            NOUS_VulkanShader::Destroy(vkContext, vs);
            return false;
        }
    }

    // ── 2. Descriptor set layouts (one per set index, ordered 0..N) ───────────
    if (!refl.descriptorSets.empty())
    {
        uint32_t maxSet = 0;
        for (const auto& [setIdx, _] : refl.descriptorSets)
            maxSet = std::max(maxSet, setIdx);

        vs->descriptorSetLayouts.resize(maxSet + 1, VK_NULL_HANDLE);

        for (const auto& [setIdx, bindings] : refl.descriptorSets)
        {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            layoutBindings.reserve(bindings.size());

            for (const ReflectedBinding& rb : bindings)
            {
                VkDescriptorSetLayoutBinding b{};
                b.binding            = rb.binding;
                b.descriptorType     = ToVkDescriptorType(rb.type);
                b.descriptorCount    = rb.count;
                b.stageFlags         = StageMaskToVkFlags(rb.stageMask);
                b.pImmutableSamplers = nullptr;
                layoutBindings.push_back(b);
            }

            VkDescriptorSetLayoutCreateInfo ci{};
            ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            ci.bindingCount = static_cast<uint32_t>(layoutBindings.size());
            ci.pBindings    = layoutBindings.data();

            VK_CHECK(vkCreateDescriptorSetLayout(dev, &ci, vkContext->allocator,
                &vs->descriptorSetLayouts[setIdx]));
        }
    }

    // ── 3. Push constant ranges from reflection ───────────────────────────────
    std::vector<VkPushConstantRange> pushRanges;
    pushRanges.reserve(refl.pushConstants.size());

    for (const ReflectedPushConstant& pc : refl.pushConstants)
    {
        VkPushConstantRange range{};
        range.stageFlags = StageMaskToVkFlags(pc.stageMask);
        range.offset     = pc.offset;
        range.size       = pc.size;
        pushRanges.push_back(range);
    }

    // ── 4. Pipeline layout ────────────────────────────────────────────────────
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount         = static_cast<uint32_t>(vs->descriptorSetLayouts.size());
    layoutCI.pSetLayouts            = vs->descriptorSetLayouts.empty()
                                        ? nullptr : vs->descriptorSetLayouts.data();
    layoutCI.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
    layoutCI.pPushConstantRanges    = pushRanges.empty() ? nullptr : pushRanges.data();

    VK_CHECK(vkCreatePipelineLayout(dev, &layoutCI, vkContext->allocator,
        &vs->pipeline.pipelineLayout));

    // ── 5. Vertex input from reflection ───────────────────────────────────────
    std::vector<ReflectedInput> sortedInputs = refl.vertexInputs;
    std::sort(sortedInputs.begin(), sortedInputs.end(),
        [](const ReflectedInput& a, const ReflectedInput& b) { return a.location < b.location; });

    std::vector<VkVertexInputAttributeDescription> attribs;
    attribs.reserve(sortedInputs.size());
    uint32_t stride = 0;

    for (const ReflectedInput& in : sortedInputs)
    {
        VkVertexInputAttributeDescription attrib{};
        attrib.location = in.location;
        attrib.binding  = 0;
        attrib.format   = DataTypeToVkFormat(in.ToDataType());
        attrib.offset   = stride;
        attribs.push_back(attrib);
        stride += in.sizeBytes;
    }

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding   = 0;
    bindingDesc.stride    = stride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // ── 6. Build the VkPipeline ───────────────────────────────────────────────
    // Viewport / scissor (dynamic states override these at draw time)
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = static_cast<float>(vkContext->framebufferHeight);
    viewport.width    = static_cast<float>(vkContext->framebufferWidth);
    viewport.height   = -static_cast<float>(vkContext->framebufferHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {static_cast<uint32_t>(vkContext->framebufferWidth),
                      static_cast<uint32_t>(vkContext->framebufferHeight)};

    VkPipelineViewportStateCreateInfo viewportStateCI{};
    viewportStateCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCI.viewportCount = 1;
    viewportStateCI.pViewports    = &viewport;
    viewportStateCI.scissorCount  = 1;
    viewportStateCI.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterCI{};
    rasterCI.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterCI.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterCI.lineWidth               = 1.0f;
    rasterCI.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterCI.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterCI.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo msaaCI{};
    msaaCI.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaaCI.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    msaaCI.sampleShadingEnable   = VK_FALSE;
    msaaCI.minSampleShading      = 1.0f;

    VkPipelineDepthStencilStateCreateInfo depthCI{};
    depthCI.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthCI.depthTestEnable  = VK_TRUE;
    depthCI.depthWriteEnable = VK_TRUE;
    depthCI.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttach.blendEnable           = VK_TRUE;
    blendAttach.srcColorBlendFactor   = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttach.dstColorBlendFactor   = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttach.colorBlendOp          = VK_BLEND_OP_ADD;
    blendAttach.srcAlphaBlendFactor   = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttach.dstAlphaBlendFactor   = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttach.alphaBlendOp          = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blendCI{};
    blendCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendCI.attachmentCount = 1;
    blendCI.pAttachments    = &blendAttach;

    std::array<VkDynamicState, 3> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo dynamicCI{};
    dynamicCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicCI.pDynamicStates    = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInputCI{};
    vertexInputCI.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCI.vertexBindingDescriptionCount   = attribs.empty() ? 0u : 1u;
    vertexInputCI.pVertexBindingDescriptions      = attribs.empty() ? nullptr : &bindingDesc;
    vertexInputCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size());
    vertexInputCI.pVertexAttributeDescriptions    = attribs.empty() ? nullptr : attribs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
    inputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCI.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

    std::vector<VkPipelineShaderStageCreateInfo> stageInfos;
    stageInfos.reserve(vs->stages.size());
    for (const VulkanShaderStage& st : vs->stages)
        stageInfos.push_back(st.shaderStageCreateInfo);

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount          = static_cast<uint32_t>(stageInfos.size());
    pipelineCI.pStages             = stageInfos.data();
    pipelineCI.pVertexInputState   = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyCI;
    pipelineCI.pViewportState      = &viewportStateCI;
    pipelineCI.pRasterizationState = &rasterCI;
    pipelineCI.pMultisampleState   = &msaaCI;
    pipelineCI.pDepthStencilState  = &depthCI;
    pipelineCI.pColorBlendState    = &blendCI;
    pipelineCI.pDynamicState       = &dynamicCI;
    pipelineCI.layout              = vs->pipeline.pipelineLayout;
    pipelineCI.renderPass          = renderpass->handle;
    pipelineCI.subpass             = 0;
    pipelineCI.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex   = -1;

    VkResult result = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1,
        &pipelineCI, vkContext->allocator, &vs->pipeline.handle);

    if (result != VK_SUCCESS)
    {
        NOUS_ERROR("[VulkanShader] vkCreateGraphicsPipelines failed (%d).", result);
        NOUS_VulkanShader::Destroy(vkContext, vs);
        return false;
    }

    shader->internalData = vs;
    NOUS_INFO("[VulkanShader] Created pipeline for shader with %zu stage(s).",
              shader->stagesData.size());

    return true;
}

// ─────────────────────────────── Destroy ─────────────────────────────────────

void NOUS_VulkanShader::Destroy(VulkanContext* vkContext, VulkanShader* vs)
{
    if (!vs) return;

    VkDevice dev                     = vkContext->device.logicalDevice;
    VkAllocationCallbacks* allocator = vkContext->allocator;

    // Pipeline and its layout
    if (vs->pipeline.handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(dev, vs->pipeline.handle, allocator);
        vs->pipeline.handle = VK_NULL_HANDLE;
    }
    if (vs->pipeline.pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(dev, vs->pipeline.pipelineLayout, allocator);
        vs->pipeline.pipelineLayout = VK_NULL_HANDLE;
    }

    // Descriptor set layouts
    for (VkDescriptorSetLayout layout : vs->descriptorSetLayouts)
    {
        if (layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(dev, layout, allocator);
    }
    vs->descriptorSetLayouts.clear();

    // Shader modules
    for (VulkanShaderStage& stage : vs->stages)
    {
        if (stage.handle != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(dev, stage.handle, allocator);
            stage.handle = VK_NULL_HANDLE;
        }
    }
    vs->stages.clear();

    NOUS_DELETE(vs, MemoryTag::RENDERER);
}

#include "VulkanShader.h"

#include "Engine/Renderer/Backend/Vulkan/Resources/GraphicsPipeline/VulkanGraphicsPipeline.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/Buffer/VulkanBuffer.h"
#include "Engine/Renderer/Backend/Vulkan/Utils/VulkanUtils.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

#include <algorithm>

// ─────────────────────────────── Module helpers ───────────────────────────────

static bool CreateShaderModuleFromBinary(VulkanContext* vkContext,
    const std::vector<uint32_t>& spirvBinary, VkShaderStageFlagBits stageFlagBit,
    VulkanShaderStage* outStage)
{
    if (!outStage || spirvBinary.empty())
    {
        NOUS_ERROR("CreateShaderModuleFromBinary: null output stage or empty binary.");
        return false;
    }

    MemoryManager::ZeroMemory(&outStage->shaderModuleCreateInfo, sizeof(VkShaderModuleCreateInfo));
    outStage->shaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    outStage->shaderModuleCreateInfo.codeSize = spirvBinary.size() * sizeof(uint32_t);
    outStage->shaderModuleCreateInfo.pCode    = spirvBinary.data();

    VK_CHECK(vkCreateShaderModule(vkContext->device.logicalDevice,
        &outStage->shaderModuleCreateInfo, vkContext->allocator, &outStage->handle));

    MemoryManager::ZeroMemory(&outStage->shaderStageCreateInfo, sizeof(VkPipelineShaderStageCreateInfo));
    outStage->shaderStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    outStage->shaderStageCreateInfo.stage  = stageFlagBit;
    outStage->shaderStageCreateInfo.module = outStage->handle;
    outStage->shaderStageCreateInfo.pName  = "main";

    return true;
}

// ─────────────────────────────── Type helpers ─────────────────────────────────

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

// Engine stage bitmask (from ShaderReflection.cpp):
//   Vertex=bit0, TessCtrl=bit1, TessEval=bit2, Geometry=bit3, Fragment=bit4, Compute=bit5
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

// ─────────────────────────────── Internal helpers ────────────────────────────

// Round size up to the device's minUniformBufferOffsetAlignment.
static uint32_t AlignUBOStride(VulkanContext* vkContext, uint32_t size)
{
    const uint64_t align = vkContext->device.properties.limits.minUniformBufferOffsetAlignment;
    if (align == 0 || size == 0) return size;
    return static_cast<uint32_t>((size + align - 1) & ~(align - 1));
}

// Allocate global (set=0) descriptor pool, one UBO buffer per image, and descriptor sets.
static bool AllocateGlobalResources(VulkanContext* vkContext, VulkanShader* vs,
                                    const PipelineReflectionResult& refl)
{
    auto it = refl.descriptorSets.find(0);
    if (it == refl.descriptorSets.end()) return true; // no set=0

    const auto& set0 = it->second;

    // Find the first UBO binding to determine the block size.
    uint32_t uboBlockSize = 0;
    for (const auto& rb : set0)
        if (rb.type == DescriptorType::UniformBuffer) { uboBlockSize = rb.blockSize; break; }

    if (uboBlockSize == 0) return true; // no UBO in set=0, nothing to allocate

    vs->globalUBOStride = uboBlockSize;

    constexpr uint32_t imageCount = 3; // triple-buffering
    VkDevice dev = vkContext->device.logicalDevice;

    // ── Descriptor pool ───────────────────────────────────────────────────────
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& rb : set0)
    {
        VkDescriptorPoolSize ps{};
        ps.type            = ToVkDescriptorType(rb.type);
        ps.descriptorCount = imageCount * rb.count;
        poolSizes.push_back(ps);
    }

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCI.pPoolSizes    = poolSizes.data();
    poolCI.maxSets       = imageCount;

    VK_CHECK(vkCreateDescriptorPool(dev, &poolCI, vkContext->allocator, &vs->globalPool));

    // ── UBO buffers (one per image) ───────────────────────────────────────────
    const uint32_t deviceLocalBits = vkContext->device.supportsDeviceLocalHostVisible
                                      ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;

    vs->globalUBOBuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, vs->globalUBOStride,
                VkBufferUsageFlagBits(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | deviceLocalBits,
                true, &vs->globalUBOBuffers[i]))
        {
            NOUS_ERROR("[VulkanShader] Failed to create global UBO buffer %u.", i);
            return false;
        }
    }

    // ── Descriptor sets (one per image) ───────────────────────────────────────
    std::vector<VkDescriptorSetLayout> layouts(imageCount, vs->descriptorSetLayouts[0]);
    vs->globalDescriptorSets.resize(imageCount);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = vs->globalPool;
    allocInfo.descriptorSetCount = imageCount;
    allocInfo.pSetLayouts        = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(dev, &allocInfo, vs->globalDescriptorSets.data()));

    // Write initial buffer descriptors so the sets are valid from the start.
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = vs->globalUBOBuffers[i].handle;
        bufInfo.offset = 0;
        bufInfo.range  = vs->globalUBOStride;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = vs->globalDescriptorSets[i];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }

    return true;
}

// Allocate instance (set=1) descriptor pool and shared UBO buffer.
static bool AllocateInstanceResources(VulkanContext* vkContext, VulkanShader* vs,
                                      const PipelineReflectionResult& refl)
{
    auto it = refl.descriptorSets.find(1);
    if (it == refl.descriptorSets.end()) return true; // no set=1

    const auto& set1 = it->second;

    uint32_t uboBlockSize = 0;
    for (const auto& rb : set1)
        if (rb.type == DescriptorType::UniformBuffer) { uboBlockSize = rb.blockSize; break; }

    vs->instanceUBOStride    = AlignUBOStride(vkContext, uboBlockSize);
    vs->instanceBindingCount = static_cast<uint32_t>(set1.size());

    constexpr uint32_t imageCount   = 3;
    constexpr uint32_t maxInstances = VULKAN_SHADER_MAX_INSTANCE_COUNT;
    VkDevice dev = vkContext->device.logicalDevice;

    // ── Descriptor pool ───────────────────────────────────────────────────────
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& rb : set1)
    {
        VkDescriptorPoolSize ps{};
        ps.type            = ToVkDescriptorType(rb.type);
        ps.descriptorCount = maxInstances * imageCount * rb.count;
        poolSizes.push_back(ps);
    }

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCI.pPoolSizes    = poolSizes.data();
    poolCI.maxSets       = maxInstances * imageCount;
    poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(dev, &poolCI, vkContext->allocator, &vs->instancePool));

    // ── Shared instance UBO buffer ────────────────────────────────────────────
    if (vs->instanceUBOStride > 0)
    {
        const uint32_t deviceLocalBits = vkContext->device.supportsDeviceLocalHostVisible
                                          ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext,
                static_cast<uint64_t>(vs->instanceUBOStride) * maxInstances,
                VkBufferUsageFlagBits(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | deviceLocalBits,
                true, &vs->instanceUBOBuffer))
        {
            NOUS_ERROR("[VulkanShader] Failed to create instance UBO buffer.");
            return false;
        }
    }

    // ── Instance state slots ──────────────────────────────────────────────────
    vs->instanceStates.resize(maxInstances);
    for (auto& state : vs->instanceStates)
    {
        state.inUse = false;
        state.descriptorStates.resize(vs->instanceBindingCount);
        for (auto& ds : state.descriptorStates)
        {
            ds.generations.fill(UINT32_MAX);
            ds.ids.fill(UINT32_MAX);
        }
    }

    return true;
}

// ─────────────────────────────── IBackendShader ───────────────────────────────

void VulkanShader::Bind()
{
    // Pipeline binding is per-draw-call via BindPipeline().
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
        if (!CreateShaderModuleFromBinary(
                vkContext, src.spirvBinary, ToVkStageFlagBit(src.stage), &vs->stages[i]))
        {
            NOUS_ERROR("[VulkanShader] Failed to create shader module for stage %d.", (int)src.stage);
            Destroy(vkContext, vs);
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

    // ── 6. Build VkGraphicsPipeline ───────────────────────────────────────────
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
    rasterCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterCI.lineWidth   = 1.0f;
    rasterCI.cullMode    = VK_CULL_MODE_BACK_BIT;
    rasterCI.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo msaaCI{};
    msaaCI.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaaCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msaaCI.sampleShadingEnable  = VK_FALSE;
    msaaCI.minSampleShading     = 1.0f;

    VkPipelineDepthStencilStateCreateInfo depthCI{};
    depthCI.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthCI.depthTestEnable  = VK_TRUE;
    depthCI.depthWriteEnable = VK_TRUE;
    depthCI.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask    = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
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

    // Detect tessellation stages so we can configure the pipeline correctly.
    bool hasTessellation = false;
    for (const ShaderSource& src : shader->stagesData)
    {
        if (src.stage == ShaderStage::TessControl || src.stage == ShaderStage::TessEvaluation)
        {
            hasTessellation = true;
            break;
        }
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
    inputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCI.topology               = hasTessellation
                                                ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
                                                : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo tessCI{};
    tessCI.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessCI.patchControlPoints = 3; // standard triangle patch

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
    pipelineCI.pTessellationState  = hasTessellation ? &tessCI : nullptr;
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
        Destroy(vkContext, vs);
        return false;
    }

    // ── 7. Allocate descriptor resources driven by reflection ─────────────────
    if (!AllocateGlobalResources(vkContext, vs, refl))
    {
        NOUS_ERROR("[VulkanShader] Failed to allocate global descriptor resources.");
        Destroy(vkContext, vs);
        return false;
    }

    if (!AllocateInstanceResources(vkContext, vs, refl))
    {
        NOUS_ERROR("[VulkanShader] Failed to allocate instance descriptor resources.");
        Destroy(vkContext, vs);
        return false;
    }

    shader->internalData = vs;
    NOUS_INFO("[VulkanShader] Created shader with %zu stage(s).", shader->stagesData.size());
    return true;
}

// ─────────────────────────────── Destroy ─────────────────────────────────────

void NOUS_VulkanShader::Destroy(VulkanContext* vkContext, VulkanShader* vs)
{
    if (!vs) return;

    VkDevice dev                     = vkContext->device.logicalDevice;
    VkAllocationCallbacks* allocator = vkContext->allocator;

    // Instance resources
    if (vs->instancePool != VK_NULL_HANDLE)
    {
        if (vs->instanceUBOBuffer.handle != VK_NULL_HANDLE)
            NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vs->instanceUBOBuffer);

        vkDestroyDescriptorPool(dev, vs->instancePool, allocator);
        vs->instancePool = VK_NULL_HANDLE;
    }
    vs->instanceStates.clear();

    // Global resources
    if (vs->globalPool != VK_NULL_HANDLE)
    {
        for (auto& buf : vs->globalUBOBuffers)
            NOUS_VulkanBuffer::DestroyBuffer(vkContext, &buf);
        vs->globalUBOBuffers.clear();

        vkDestroyDescriptorPool(dev, vs->globalPool, allocator);
        vs->globalPool = VK_NULL_HANDLE;
    }

    // Pipeline and layout
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
        if (layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(dev, layout, allocator);
    vs->descriptorSetLayouts.clear();

    // Shader modules
    for (VulkanShaderStage& stage : vs->stages)
        if (stage.handle != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(dev, stage.handle, allocator);
            stage.handle = VK_NULL_HANDLE;
        }
    vs->stages.clear();

    NOUS_DELETE(vs, MemoryTag::RENDERER);
}

// ─────────────────────────────── Per-frame ───────────────────────────────────

void NOUS_VulkanShader::BindPipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs)
{
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vs->pipeline.handle);
}

void NOUS_VulkanShader::UpdateGlobal(VulkanContext* vkContext, VkCommandBuffer cmdBuffer,
                                      VulkanShader* vs, uint32_t imageIndex,
                                      const void* data, uint64_t size)
{
    if (!vs->globalPool || imageIndex >= vs->globalDescriptorSets.size()) return;

    NOUS_VulkanBuffer::LoadData(vkContext, &vs->globalUBOBuffers[imageIndex],
        0, size, 0, const_cast<void*>(data));

    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = vs->globalUBOBuffers[imageIndex].handle;
    bufInfo.offset = 0;
    bufInfo.range  = vs->globalUBOStride;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = vs->globalDescriptorSets[imageIndex];
    write.dstBinding      = 0;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo     = &bufInfo;

    vkUpdateDescriptorSets(vkContext->device.logicalDevice, 1, &write, 0, nullptr);

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        vs->pipeline.pipelineLayout, 0, 1,
        &vs->globalDescriptorSets[imageIndex], 0, nullptr);
}

// ─────────────────────────────── Instance management ─────────────────────────

bool NOUS_VulkanShader::AcquireInstanceSlot(VulkanContext* vkContext, VulkanShader* vs,
                                             uint32_t* outID)
{
    if (!vs->instancePool)
    {
        NOUS_ERROR("[VulkanShader] AcquireInstanceSlot: no instance pool.");
        return false;
    }

    for (uint32_t i = 0; i < VULKAN_SHADER_MAX_INSTANCE_COUNT; ++i)
    {
        if (vs->instanceStates[i].inUse) continue;

        // Allocate three descriptor sets (one per swapchain image).
        const VkDescriptorSetLayout layout = vs->descriptorSetLayouts[1];
        std::array<VkDescriptorSetLayout, 3> layouts = {layout, layout, layout};

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = vs->instancePool;
        allocInfo.descriptorSetCount = 3;
        allocInfo.pSetLayouts        = layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(vkContext->device.logicalDevice,
            &allocInfo, vs->instanceStates[i].descriptorSets.data()));

        for (auto& ds : vs->instanceStates[i].descriptorStates)
        {
            ds.generations.fill(UINT32_MAX);
            ds.ids.fill(UINT32_MAX);
        }

        vs->instanceStates[i].inUse = true;
        *outID = i;
        return true;
    }

    NOUS_ERROR("[VulkanShader] Instance pool full (%u slots).", VULKAN_SHADER_MAX_INSTANCE_COUNT);
    return false;
}

void NOUS_VulkanShader::ReleaseInstanceSlot(VulkanContext* vkContext, VulkanShader* vs,
                                             uint32_t id)
{
    if (id >= VULKAN_SHADER_MAX_INSTANCE_COUNT || !vs->instanceStates[id].inUse) return;

    VK_CHECK(vkFreeDescriptorSets(vkContext->device.logicalDevice, vs->instancePool,
        3, vs->instanceStates[id].descriptorSets.data()));

    vs->instanceStates[id].inUse = false;
}

// ─────────────────────────────── Descriptor writes ───────────────────────────

void NOUS_VulkanShader::WriteInstanceUBO(VulkanContext* vkContext, VulkanShader* vs,
                                          uint32_t imageIndex, uint32_t instanceID,
                                          const void* data, uint64_t size,
                                          uint32_t* inOutGeneration)
{
    if (!vs->instancePool || instanceID >= VULKAN_SHADER_MAX_INSTANCE_COUNT) return;
    if (!vs->instanceStates[instanceID].inUse) return;

    const uint64_t offset = static_cast<uint64_t>(instanceID) * vs->instanceUBOStride;
    NOUS_VulkanBuffer::LoadData(vkContext, &vs->instanceUBOBuffer,
        offset, size, 0, const_cast<void*>(data));

    // Write descriptor only once or when invalidated.
    if (*inOutGeneration == UINT32_MAX)
    {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = vs->instanceUBOBuffer.handle;
        bufInfo.offset = offset;
        bufInfo.range  = vs->instanceUBOStride;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = vs->instanceStates[instanceID].descriptorSets[imageIndex];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(vkContext->device.logicalDevice, 1, &write, 0, nullptr);
        *inOutGeneration = 0;
    }
}

void NOUS_VulkanShader::WriteInstanceSampler(VulkanContext* vkContext, VulkanShader* vs,
                                              uint32_t imageIndex, uint32_t instanceID,
                                              uint32_t bindingIndex,
                                              VkImageView imageView, VkSampler sampler,
                                              uint32_t* inOutGeneration, uint32_t* inOutID,
                                              uint32_t resourceID, uint32_t resourceGeneration)
{
    if (!vs->instancePool || instanceID >= VULKAN_SHADER_MAX_INSTANCE_COUNT) return;

    // A generation of UINT32_MAX means this binding has never been written — always
    // force the first write even if the resource ID/generation happen to match that
    // sentinel (e.g. uninitialized ResourceTexture::ID / generation fields).
    const bool neverWritten = (*inOutGeneration == UINT32_MAX);

    // Lazy: skip write if resource is unchanged (only after at least one write).
    if (!neverWritten && *inOutID == resourceID && *inOutGeneration == resourceGeneration) return;

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgInfo.imageView   = imageView;
    imgInfo.sampler     = sampler;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = vs->instanceStates[instanceID].descriptorSets[imageIndex];
    write.dstBinding      = bindingIndex;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &imgInfo;

    vkUpdateDescriptorSets(vkContext->device.logicalDevice, 1, &write, 0, nullptr);

    *inOutGeneration = resourceGeneration;
    *inOutID         = resourceID;
}

void NOUS_VulkanShader::BindInstanceDescriptorSet(VkCommandBuffer cmdBuffer, VulkanShader* vs,
                                                   uint32_t imageIndex, uint32_t instanceID)
{
    if (!vs->instancePool || instanceID >= VULKAN_SHADER_MAX_INSTANCE_COUNT) return;

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        vs->pipeline.pipelineLayout, 1, 1,
        &vs->instanceStates[instanceID].descriptorSets[imageIndex], 0, nullptr);
}

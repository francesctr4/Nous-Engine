#include "VulkanObjectShader.h"
#include "VulkanShaderUtils.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanExternal.h"
#include "VulkanUtils.h"
#include "VulkanBuffer.h"

#include "Logger.h"
#include "MemoryManager.h"

constexpr const char* BUILTIN_OBJECT_SHADER_NAME = "BuiltIn.ObjectShader";

bool CreateObjectShader(VulkanContext* vkContext, Texture* defaultDiffuse, VulkanObjectShader* outShader)
{
#ifdef _DEBUG
    ExecuteBatchFile("compile-shaders.bat");
#endif // _DEBUG

    bool ret = true;

    outShader->defaultDiffuse = defaultDiffuse;

    // Shader module init per stage.
    std::array<std::string, VULKAN_OBJECT_SHADER_STAGE_COUNT> stageTypeStrings = { "vert", "frag" };
    std::array<VkShaderStageFlagBits, VULKAN_OBJECT_SHADER_STAGE_COUNT> stageTypes = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

    for (uint32 i = 0; i < VULKAN_OBJECT_SHADER_STAGE_COUNT; ++i)
    {
        if (!CreateShaderModule(vkContext, BUILTIN_OBJECT_SHADER_NAME, stageTypeStrings[i], stageTypes[i], i, outShader->stages.data()))
        {
            NOUS_ERROR("Unable to create %s shader module for '%s'.", stageTypeStrings[i].c_str(), BUILTIN_OBJECT_SHADER_NAME);
            ret = false;
        }
    }

    // Global Descriptors
    VkDescriptorSetLayoutBinding globalUboLayoutBinding;

    globalUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    globalUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    globalUboLayoutBinding.binding = 0;
    globalUboLayoutBinding.descriptorCount = 1;
    globalUboLayoutBinding.pImmutableSamplers = 0;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &globalUboLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(vkContext->device.logicalDevice, &descriptorSetLayoutCreateInfo, vkContext->allocator, &outShader->globalDescriptorSetLayout));

    // Global descriptor pool: Used for global items such as view/projection matrix.
    VkDescriptorPoolSize globalDescriptorPoolSize;
    globalDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    globalDescriptorPoolSize.descriptorCount = vkContext->swapChain.swapChainImages.size();

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &globalDescriptorPoolSize;

    descriptorPoolCreateInfo.maxSets = vkContext->swapChain.swapChainImages.size();

    VK_CHECK(vkCreateDescriptorPool(vkContext->device.logicalDevice, &descriptorPoolCreateInfo, vkContext->allocator, &outShader->globalDescriptorPool));

    // Local/Object Descriptors
    const uint32 LOCAL_SAMPLER_COUNT = 1;

    std::array<VkDescriptorType, VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT> descriptorTypes =
    {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          // Binding 0 - uniform buffer
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // Binding 1 - Diffuse sampler layout.
    };

    std::array<VkDescriptorSetLayoutBinding, VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT> bindings;
    MemoryManager::ZeroMemory(&bindings, sizeof(VkDescriptorSetLayoutBinding) * VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT);

    for (uint32 i = 0; i < VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT; ++i) 
    {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].descriptorType = descriptorTypes[i];
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo localDescriptorSetLayoutCreateInfo{};
    localDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    localDescriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32>(bindings.size());
    localDescriptorSetLayoutCreateInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(vkContext->device.logicalDevice, &localDescriptorSetLayoutCreateInfo, vkContext->allocator, &outShader->localDescriptorSetLayout));

    // Local/Object descriptor pool: Used for object-specific items like diffuse colour
    std::array<VkDescriptorPoolSize, VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT> localDescriptorPoolSizes;

    // The first section will be used for uniform buffers
    localDescriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    localDescriptorPoolSizes[0].descriptorCount = VULKAN_OBJECT_SHADER_MAX_OBJECT_COUNT;

    // The second section will be used for image samplers.
    localDescriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    localDescriptorPoolSizes[1].descriptorCount = LOCAL_SAMPLER_COUNT * VULKAN_OBJECT_SHADER_MAX_OBJECT_COUNT;

    VkDescriptorPoolCreateInfo localDescriptorPoolCreateInfo{};
    localDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    localDescriptorPoolCreateInfo.poolSizeCount = static_cast<uint32>(localDescriptorPoolSizes.size());
    localDescriptorPoolCreateInfo.pPoolSizes = localDescriptorPoolSizes.data();
    localDescriptorPoolCreateInfo.maxSets = VULKAN_OBJECT_SHADER_MAX_OBJECT_COUNT;

    // Create object descriptor pool.
    VK_CHECK(vkCreateDescriptorPool(vkContext->device.logicalDevice, &localDescriptorPoolCreateInfo, vkContext->allocator, &outShader->localDescriptorPool));

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

    // Desciptor set layouts
    const int32 DESCRIPTOR_SET_LAYOUT_COUNT = 2;
    std::array<VkDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_COUNT> descriptorSetlayouts = 
    { 
        outShader->globalDescriptorSetLayout,
        outShader->localDescriptorSetLayout
    };
    
    // Stages
    // NOTE: Should match the number of shader->stages.
    std::array<VkPipelineShaderStageCreateInfo, VULKAN_OBJECT_SHADER_STAGE_COUNT> shaderStageCreateInfos;
    MemoryManager::ZeroMemory(shaderStageCreateInfos.data(), sizeof(shaderStageCreateInfos));

    for (u32 i = 0; i < VULKAN_OBJECT_SHADER_STAGE_COUNT; ++i)
    {
        shaderStageCreateInfos[i].sType = outShader->stages[i].shaderStageCreateInfo.sType;
        shaderStageCreateInfos[i] = outShader->stages[i].shaderStageCreateInfo;
    }

    if (!CreateGraphicsPipeline(vkContext, &vkContext->mainRenderpass, bindingDescription, 
        static_cast<uint32>(attributeDescriptions.size()), attributeDescriptions.data(),
        static_cast<uint32>(descriptorSetlayouts.size()), descriptorSetlayouts.data(), 
        VULKAN_OBJECT_SHADER_STAGE_COUNT, shaderStageCreateInfos.data(), viewport, scissor,
        false, &outShader->pipeline))
    {
        NOUS_ERROR("Failed to load graphics pipeline for object shader.");

        ret = false;
    }

    // Create uniform buffer.
    if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, sizeof(GlobalUniformObject) * 3,
        VkBufferUsageFlagBits(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true, &outShader->globalUniformBuffer)) 
    {
        NOUS_ERROR("Vulkan Buffer Creation failed for object shader.");
        return false;
    }

    // Allocate global descriptor sets.
    const int32 GLOBAL_DESCRIPTOR_SET_LAYOUTS_COUNT = 3;
    std::array<VkDescriptorSetLayout, GLOBAL_DESCRIPTOR_SET_LAYOUTS_COUNT> globalDescriptorSetLayouts =
    {
        outShader->globalDescriptorSetLayout,
        outShader->globalDescriptorSetLayout,
        outShader->globalDescriptorSetLayout
    };

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    descriptorSetAllocateInfo.descriptorPool = outShader->globalDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32>(globalDescriptorSetLayouts.size());
    descriptorSetAllocateInfo.pSetLayouts = globalDescriptorSetLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(vkContext->device.logicalDevice, &descriptorSetAllocateInfo, outShader->globalDescriptorSets.data()));

    // Create the object uniform buffer.
    if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, sizeof(LocalUniformObject),  //* MAX_MATERIAL_INSTANCE_COUNT,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true, &outShader->localUniformBuffer)) 
    {
        NOUS_ERROR("Material instance buffer creation failed for shader.");
        ret = false;
    }

    return ret;
}

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    VkDevice logicalDevice = vkContext->device.logicalDevice;

    // Destroy local descriptor pool and set layouts.
    vkDestroyDescriptorPool(logicalDevice, shader->localDescriptorPool, vkContext->allocator);
    vkDestroyDescriptorSetLayout(logicalDevice, shader->localDescriptorSetLayout, vkContext->allocator);

    // Destroy global descriptor pool and set layouts.
    vkDestroyDescriptorPool(logicalDevice, shader->globalDescriptorPool, vkContext->allocator);
    vkDestroyDescriptorSetLayout(logicalDevice, shader->globalDescriptorSetLayout, vkContext->allocator);

    // Destroy global and local uniform buffers
    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &shader->localUniformBuffer);
    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &shader->globalUniformBuffer);

    // Destroy pipeline.
    DestroyGraphicsPipeline(vkContext, &shader->pipeline);

    NOUS_DEBUG("Destroying Shader Modules...");
    // Destroy shader modules.
    for (uint32 i = 0; i < VULKAN_OBJECT_SHADER_STAGE_COUNT; ++i)
    {
        vkDestroyShaderModule(logicalDevice, shader->stages[i].handle, vkContext->allocator);
        shader->stages[i].handle = 0;
    }
}

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader)
{
    BindGraphicsPipeline(&vkContext->graphicsCommandBuffers[vkContext->imageIndex],
        VK_PIPELINE_BIND_POINT_GRAPHICS, &shader->pipeline);
}

void UpdateObjectShaderGlobalState(VulkanContext* vkContext, VulkanObjectShader* shader, float deltaTime)
{
    u32 imageIndex = vkContext->imageIndex;
    VkCommandBuffer commandBuffer = vkContext->graphicsCommandBuffers[imageIndex].handle;
    VkDescriptorSet globalDescriptorSet = shader->globalDescriptorSets[imageIndex];

    // Configure the descriptors for the given index.
    u32 range = sizeof(GlobalUniformObject);
    u64 offset = sizeof(GlobalUniformObject) * imageIndex;

    // Copy data to buffer
    NOUS_VulkanBuffer::LoadBuffer(vkContext, &shader->globalUniformBuffer, offset, range, 0, &shader->globalUBO);

    VkDescriptorBufferInfo descriptorBufferInfo;
    descriptorBufferInfo.buffer = shader->globalUniformBuffer.handle;
    descriptorBufferInfo.offset = offset;
    descriptorBufferInfo.range = range;

    // Update descriptor sets.
    VkWriteDescriptorSet writeDescriptorSetInfo{};
    writeDescriptorSetInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    writeDescriptorSetInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSetInfo.descriptorCount = 1;

    writeDescriptorSetInfo.dstSet = shader->globalDescriptorSets[imageIndex];
    writeDescriptorSetInfo.dstBinding = 0;
    writeDescriptorSetInfo.dstArrayElement = 0;

    writeDescriptorSetInfo.pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(vkContext->device.logicalDevice, 1, &writeDescriptorSetInfo, 0, 0);

    // Bind the global descriptor set to be updated.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline.pipelineLayout, 0, 1, &globalDescriptorSet, 0, 0);
}

void UpdateObjectShaderLocalState(VulkanContext* vkContext, VulkanObjectShader* shader, GeometryRenderData renderData)
{
    uint32 imageIndex = vkContext->imageIndex;

    VkCommandBuffer commandBuffer = vkContext->graphicsCommandBuffers[imageIndex].handle;

    vkCmdPushConstants(commandBuffer, shader->pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4x4), &renderData.model);

    // Obtain material data.
    VulkanObjectShaderLocalState* objectState = &shader->localObjectStates[renderData.objectID];
    VkDescriptorSet objectDescriptorSet = objectState->descriptorSets[imageIndex];

    // TODO: if needs update
    std::array<VkWriteDescriptorSet, VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT> descriptorWrites;
    MemoryManager::ZeroMemory(descriptorWrites.data(), sizeof(VkWriteDescriptorSet) * static_cast<uint32>(descriptorWrites.size()));
    uint32 descriptorCount = 0;
    uint32 descriptorIndex = 0;

    // Descriptor 0 - Uniform buffer
    uint32 range = sizeof(LocalUniformObject);
    uint64 offset = sizeof(LocalUniformObject) * renderData.objectID;  // Also the index into the array.
    LocalUniformObject localUBO;

    // TODO: get diffuse color from a material.
    static float accumulator = 0.0f;
    accumulator += vkContext->frameDeltaTime;

    float s = (sin(accumulator) + 1.0f) / 2.0f;  // scale from -1, 1 to 0, 1
    localUBO.diffuseColor = float4(s, s, s, 1.0f);

    // Load the data into the buffer.
    NOUS_VulkanBuffer::LoadBuffer(vkContext, &shader->localUniformBuffer, offset, range, 0, &localUBO);

    // Only do this if the descriptor has not yet been updated.
    if (objectState->descriptorStates[descriptorIndex].generations[imageIndex] == INVALID_ID) 
    {
        VkDescriptorBufferInfo descriptorBufferInfo{};
        descriptorBufferInfo.buffer = shader->localUniformBuffer.handle;
        descriptorBufferInfo.offset = offset;
        descriptorBufferInfo.range = range;

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        writeDescriptorSet.dstSet = objectDescriptorSet;
        writeDescriptorSet.dstBinding = descriptorIndex;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;

        descriptorWrites[descriptorCount] = writeDescriptorSet;
        descriptorCount++;

        // Update the frame generation. In this case it is only needed once since this is a buffer.
        objectState->descriptorStates[descriptorIndex].generations[imageIndex] = 1;
    }

    descriptorIndex++;

    // TODO: samplers.
    const u32 SAMPLER_COUNT = 1;
    std::array<VkDescriptorImageInfo, SAMPLER_COUNT> descriptorImageInfo;

    for (uint32 samplerIndex = 0; samplerIndex < SAMPLER_COUNT; ++samplerIndex)
    {
        Texture* texture = renderData.textures[samplerIndex];

        uint32* descriptorGeneration = &objectState->descriptorStates[descriptorIndex].generations[imageIndex];
        
        // If the texture hasn't been loaded yet, use the default.
        // TODO: Determine which use the texture has and pull appropriate default based on that.
        if (texture->generation == INVALID_ID) 
        {
            texture = shader->defaultDiffuse;

            // Reset the descriptor generation if using the default texture.
            *descriptorGeneration = INVALID_ID;
        }

        // Check if the descriptor needs updating first.
        if (texture && (*descriptorGeneration != texture->generation || *descriptorGeneration == INVALID_ID)) 
        {
            VulkanTextureData* textureData = (VulkanTextureData*)texture->internalData;

            // Assign view and sampler.
            descriptorImageInfo[samplerIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo[samplerIndex].imageView = textureData->image.view;
            descriptorImageInfo[samplerIndex].sampler = textureData->sampler;

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

            writeDescriptorSet.dstSet = objectDescriptorSet;
            writeDescriptorSet.dstBinding = descriptorIndex;

            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet.descriptorCount = 1;

            writeDescriptorSet.pImageInfo = &descriptorImageInfo[samplerIndex];

            descriptorWrites[descriptorCount] = writeDescriptorSet;
            descriptorCount++;

            // Sync frame generation if not using a default texture.
            if (texture->generation != INVALID_ID) 
            {
                *descriptorGeneration = texture->generation;
            }

            descriptorIndex++;

        }

    }

    if (descriptorCount > 0) 
    {
        vkUpdateDescriptorSets(vkContext->device.logicalDevice, descriptorCount, descriptorWrites.data(), 0, 0);
    }

    // Bind the descriptor set to be updated, or in case the shader changed.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline.pipelineLayout, 1, 1, &objectDescriptorSet, 0, 0);
}

bool AcquireObjectShaderResources(VulkanContext* vkContext, VulkanObjectShader* shader, uint32* outObjectID)
{
    // TODO: free list
    *outObjectID = shader->localUniformBufferIndex;
    shader->localUniformBufferIndex++;

    uint32 objectID = *outObjectID;

    VulkanObjectShaderLocalState* objectState = &shader->localObjectStates[objectID];
    uint32 descriptorSetCount = static_cast<uint32>(objectState->descriptorSets.size());

    for (uint32 i = 0; i < VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT; ++i)
    {
        for (u32 j = 0; j < descriptorSetCount; ++j)
        {
            objectState->descriptorStates[i].generations[j] = INVALID_ID;
        }
    }

    // Allocate descriptor sets.
    std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts =
    {
        shader->localDescriptorSetLayout,
        shader->localDescriptorSetLayout,
        shader->localDescriptorSetLayout
    };

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    descriptorSetAllocateInfo.descriptorPool = shader->localDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;  // one per frame
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(vkContext->device.logicalDevice, &descriptorSetAllocateInfo, objectState->descriptorSets.data()));

    return true;
}

void ReleaseObjectShaderResources(VulkanContext* vkContext, VulkanObjectShader* shader, uint32 objectID)
{
    VulkanObjectShaderLocalState* objectState = &shader->localObjectStates[objectID];

    uint32 descriptorSetCount = static_cast<uint32>(objectState->descriptorSets.size());

    // Release object descriptor sets.
    VK_CHECK(vkFreeDescriptorSets(vkContext->device.logicalDevice, shader->localDescriptorPool, descriptorSetCount, objectState->descriptorSets.data()));

    for (uint32 i = 0; i < VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT; ++i) 
    {
        for (u32 j = 0; j < descriptorSetCount; ++j)
        {
            objectState->descriptorStates[i].generations[j] = INVALID_ID;
        }
    }

    // TODO: add the object_id to the free list
}
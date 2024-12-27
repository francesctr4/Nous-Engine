#include "VulkanBackend.h"
#include "VulkanTypes.inl"

#include "VulkanExternal.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanRenderpass.h"
#include "VulkanCommandBuffer.h"
#include "VulkanFramebuffer.h"
#include "VulkanSyncObjects.h"
#include "VulkanUtils.h"
#include "VulkanDebugMessenger.h"
#include "VulkanInstance.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanShaderUtils.h"

// Shaders
#include "VulkanMaterialShader.h"

// Temp
#include "ImporterMesh.h"
Mesh* myMesh;
// Temp

#include "MemoryManager.h"
#include "Logger.h"

VulkanContext* VulkanBackend::vkContext = nullptr;

VulkanBackend::VulkanBackend()
{
    vkContext = NOUS_NEW<VulkanContext>(MemoryManager::MemoryTag::RENDERER);
}

VulkanBackend::~VulkanBackend()
{
    NOUS_DELETE(vkContext, MemoryManager::MemoryTag::RENDERER);
}

bool VulkanBackend::Initialize()
{
    bool ret = true;

    NOUS_INFO(" ----------------------- USING VULKAN BACKEND ----------------------- ");

    // TODO: Custom allocator
    vkContext->allocator = 0;

    // Get Framebuffer Size
    GetFramebufferSize(&cachedFramebufferWidth, &cachedFramebufferHeight);

    vkContext->framebufferWidth = (cachedFramebufferWidth != 0) ? cachedFramebufferWidth : WINDOW_WIDTH;
    vkContext->framebufferHeight = (cachedFramebufferHeight != 0) ? cachedFramebufferHeight : WINDOW_HEIGHT;

    cachedFramebufferWidth = 0;
    cachedFramebufferWidth = 0;

    // Instance
    NOUS_DEBUG("Creating Vulkan instance...");
    if (!NOUS_VulkanInstance::CreateInstance(vkContext)) 
    {
        NOUS_ERROR("Failed to create Vulkan Instance. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG("Vulkan Instance created successfully!");
    }

    // Debugger
    NOUS_DEBUG("Creating Vulkan Debugger...");
    if (!NOUS_VulkanDebugMessenger::SetupDebugMessenger(vkContext)) 
    {
        NOUS_ERROR("Failed to create Vulkan Debugger. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG("Vulkan Debugger created successfully!");
    }

    // Surface
    NOUS_DEBUG("Creating Vulkan surface...");
    if (!NOUS_VulkanInstance::CreateSurface(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Surface. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG("Vulkan Surface created successfully!");
    }

    // Physical Device
    NOUS_DEBUG("Searching for a suitable Physical Device...");
    if (!PickPhysicalDevice(vkContext))
    {
        NOUS_ERROR("Failed to find a suitable GPU!");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Suitable GPU found!");
    }

    // Logical Device
    NOUS_DEBUG("Creating Vulkan Logical Device...");
    if (!CreateLogicalDevice(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Logical Device. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Logical Device created successfully!");
    }

    // Swap Chain
    NOUS_DEBUG("Creating Vulkan Swap Chain...");
    if (!CreateSwapChain(vkContext, vkContext->framebufferWidth, vkContext->framebufferHeight, &vkContext->swapChain))
    {
        NOUS_ERROR("Failed to create Vulkan Swap Chain. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Swap Chain created successfully!");
    }

    // Render Pass
    NOUS_DEBUG("Creating Vulkan Render Pass...");
    if (!CreateRenderpass(vkContext, &vkContext->mainRenderpass,
        0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight,
        0.1f, 0.0f, 0.0f, 1.0f,
        1.0f,
        0))
    {
        NOUS_ERROR("Failed to create Vulkan Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Render Pass created successfully!");
    }

    // Swapchain Framebuffers
    NOUS_DEBUG("Creating Vulkan Swapchain Framebuffers...");
    if (!NOUS_VulkanFramebuffer::CreateFramebuffers(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Swapchain Framebuffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Swapchain Framebuffers created successfully!");
    }

    // Create Command Buffers
    NOUS_DEBUG("Creating Vulkan Command Buffers...");
    if (!NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Command Buffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Command Buffers created successfully!");
    }

    // Create Sync Objects
    NOUS_DEBUG("Creating Vulkan Sync Objects...");
    if (!NOUS_VulkanSyncObjects::CreateSyncObjects(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Sync Objects. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Sync Objects created successfully!");
    }

    // Create Vulkan Material Shader
    NOUS_DEBUG("Creating Nous Material Shader...");
    if (!CreateMaterialShader(vkContext, &vkContext->materialShader))
    {
        NOUS_ERROR("Failed to create Nous Material Shader. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Nous Material Shader created successfully!");
    }

    // Create Vulkan Buffers
    NOUS_DEBUG("Creating Vulkan Buffers...");
    if (!NOUS_VulkanBuffer::CreateBuffers(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Buffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Buffers created successfully!");
    }

#pragma region TEMPORAL DRAW CODE

    // TODO: Temporary Test Code //

    const uint32 nVertices = 4;
    std::vector<Vertex> vertices(nVertices);
    MemoryManager::ZeroMemory(vertices.data(), sizeof(Vertex) * nVertices);

    const float factor = 10.0f;

    // Bottom-left
    vertices[0].position = float3({ -0.5f, -0.5f, 0.0f }) * factor;
    vertices[0].color = { 1.0f, 0.0f, 0.0f }; // Red
    vertices[0].texCoord = { 0.0f, 0.0f };

    // Top-Right
    vertices[1].position = float3({ 0.5f, 0.5f, 0.0f }) * factor;
    vertices[1].color = { 0.0f, 1.0f, 0.0f }; // Green
    vertices[1].texCoord = { 1.0f, 1.0f };

    // Top-Left
    vertices[2].position = float3({ -0.5f, 0.5f, 0.0f }) * factor;
    vertices[2].color = { 0.0f, 0.0f, 1.0f }; // Blue
    vertices[2].texCoord = { 0.0f, 1.0f };

    // Bottom-right
    vertices[3].position = float3({ 0.5f, -0.5f, 0.0f }) * factor;
    vertices[3].color = { 1.0f, 1.0f, 0.0f }; // Yellow
    vertices[3].texCoord = { 1.0f, 0.0f };

    const uint32 nIndices = 6;
    std::vector<uint32> indices = { 0, 1, 2, 0, 3, 1 };

    myMesh = NOUS_NEW<Mesh>(MemoryManager::MemoryTag::GAME);

    ImporterMesh::Import("Assets/Models/Cypher_S0_Skelmesh.fbx", myMesh);
    ImporterMesh::Save("Library/Models/Cypher_S0_Skelmesh.nmesh", *myMesh);
    
    NOUS_DELETE(myMesh, MemoryManager::MemoryTag::GAME);
    myMesh = NOUS_NEW<Mesh>(MemoryManager::MemoryTag::GAME);

    ImporterMesh::Load("Library/Models/Cypher_S0_Skelmesh.nmesh", myMesh);

    NOUS_VulkanBuffer::UploadDataToBuffer(vkContext, vkContext->device.graphicsCommandPool, 0, 
        vkContext->device.graphicsQueue, &vkContext->objectVertexBuffer, 0,
        sizeof(Vertex) * myMesh->vertices.size(), myMesh->vertices.data());

    NOUS_VulkanBuffer::UploadDataToBuffer(vkContext, vkContext->device.graphicsCommandPool, 0,
        vkContext->device.graphicsQueue, &vkContext->objectIndexBuffer, 0,
        sizeof(uint32) * myMesh->indices.size(), myMesh->indices.data());

    uint32 objectID = 0;
    if (!AcquireMaterialShaderResources(vkContext, &vkContext->materialShader, &objectID)) 
    {
        NOUS_ERROR("Failed to Acquire Shader Resources.");
        ret = false;
    }

    // TODO: End Temp Test Code //

#pragma endregion

	return ret;
}

void VulkanBackend::Shutdown()
{
    // Temp
    myMesh->vertices.clear();
    myMesh->indices.clear();
    NOUS_DELETE(myMesh, MemoryManager::MemoryTag::GAME);
    // Temp

    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    NOUS_VulkanBuffer::DestroyBuffers(vkContext);

    DestroyMaterialShader(vkContext, &vkContext->materialShader);

    NOUS_VulkanSyncObjects::DestroySyncObjects(vkContext);

    NOUS_VulkanCommandBuffer::DestroyCommandBuffers(vkContext);

    NOUS_VulkanFramebuffer::DestroyFramebuffers(vkContext);

    DestroyRenderpass(vkContext, &vkContext->mainRenderpass);

    DestroySwapChain(vkContext, &vkContext->swapChain);

    DestroyLogicalDevice(vkContext);

    NOUS_VulkanInstance::DestroySurface(vkContext);

    NOUS_VulkanDebugMessenger::DestroyDebugUtilsMessengerEXT(vkContext->instance, vkContext->debugMessenger, vkContext->allocator);

    NOUS_VulkanInstance::DestroyInstance(vkContext);
}

void VulkanBackend::Resized(uint16 width, uint16 height)
{
    // Update the "framebuffer size generation", a counter which indicates when the
    // framebuffer size has been updated.

    cachedFramebufferWidth = width;
    cachedFramebufferHeight = height;

    vkContext->framebufferSizeGeneration++;

    NOUS_INFO("Vulkan Renderer Backend --> Resized: W / H / GEN: %i / %i / %llu", width, height, vkContext->framebufferSizeGeneration);
}

bool VulkanBackend::BeginFrame(float dt)
{
    vkContext->frameDeltaTime = dt;

    VulkanDevice* device = &vkContext->device;

    if (vkContext->recreatingSwapchain) // Check if recreating swap chain and boot out.
    {
        VkResult result = vkDeviceWaitIdle(device->logicalDevice);

        if (!VkResultIsSuccess(result)) 
        {
            NOUS_ERROR("VulkanBackend::BeginFrame() --> vkDeviceWaitIdle (1) failed: '%s'", VkResultMessage(result, true));
            return false;
        }

        NOUS_INFO("Recreating Vulkan Swapchain, booting.");
        return false;
    }

    // Check if the Framebuffer has been resized. If so, a new swapchain must be created.

    if (vkContext->framebufferSizeGeneration != vkContext->framebufferSizeLastGeneration)
    {
        VkResult result = vkDeviceWaitIdle(device->logicalDevice);

        if (!VkResultIsSuccess(result))
        {
            NOUS_ERROR("VulkanBackend::BeginFrame() --> vkDeviceWaitIdle (2) failed: '%s'", VkResultMessage(result, true));
            return false;
        }

        // If the swapchain recreation failed (because, for example, the window was minimized),
        // boot out before unsetting the flag.
        if (!RecreateResources()) 
        {
            return false;
        }

        NOUS_INFO("Resized, booting.");
        return false;
    }

    // Wait for the execution of the current frame to complete. The fence being free will allow this one to move on.
    if (!NOUS_VulkanSyncObjects::WaitOnVulkanFence(vkContext, &vkContext->inFlightFences[vkContext->currentFrame], UINT64_MAX))
    {
        NOUS_WARN("In-flight fence wait failure!");
        return false;
    }

    // Acquire the next image from the swap chain. Pass along the semaphore that should signaled when this completes.
    // This same semaphore will later be waited on by the queue submission to ensure this image is available.
    if (!SwapChainAcquireNextImageIndex(vkContext, &vkContext->swapChain, UINT64_MAX,
        vkContext->imageAvailableSemaphores[vkContext->currentFrame], 0, &vkContext->imageIndex))
    {
        return false;
    }

    // Begin recording commands.
    VulkanCommandBuffer* commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];

    NOUS_VulkanCommandBuffer::CommandBufferReset(commandBuffer);
    NOUS_VulkanCommandBuffer::CommandBufferBegin(commandBuffer, false, false, false);

    // Dynamic state
    VkViewport viewport;

    viewport.x = 0.0f;
    viewport.y = (float)vkContext->framebufferHeight;

    viewport.width = (float)vkContext->framebufferWidth;
    viewport.height = -(float)vkContext->framebufferHeight;

    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);

    // Scissor
    VkRect2D scissor;

    scissor.offset.x = 0;
    scissor.offset.y = 0;

    scissor.extent.width = vkContext->framebufferWidth;
    scissor.extent.height = vkContext->framebufferHeight;
    
    vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissor);

    vkContext->mainRenderpass.w = vkContext->framebufferWidth;
    vkContext->mainRenderpass.h = vkContext->framebufferHeight;

    // Begin the render pass
    BeginRenderpass(commandBuffer, &vkContext->mainRenderpass,
        vkContext->swapChain.swapChainFramebuffers[vkContext->imageIndex].handle);

    return true;
}

bool VulkanBackend::EndFrame(float dt)
{
    VulkanCommandBuffer* commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];

    // End renderpass
    EndRenderpass(commandBuffer, &vkContext->mainRenderpass);
    NOUS_VulkanCommandBuffer::CommandBufferEnd(commandBuffer);

    // Make sure the previous frame is not using this image (i.e. its fence is being waited on)
    if (vkContext->imagesInFlight[vkContext->imageIndex] != VK_NULL_HANDLE) // was frame
    {  
        NOUS_VulkanSyncObjects::WaitOnVulkanFence(vkContext, vkContext->imagesInFlight[vkContext->imageIndex], UINT64_MAX);
    }

    // Mark the image fence as in-use by this frame.
    vkContext->imagesInFlight[vkContext->imageIndex] = &vkContext->inFlightFences[vkContext->currentFrame];

    // Reset the fence for use on the next frame
    NOUS_VulkanSyncObjects::ResetVulkanFence(vkContext, &vkContext->inFlightFences[vkContext->currentFrame]);

    // Submit the queue and wait for the operation to complete.
    // Begin queue submission
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Command buffer(s) to be executed.
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer->handle;

    // The semaphore(s) to be signaled when the queue is complete.
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vkContext->queueCompleteSemaphores[vkContext->currentFrame];

    // Wait semaphore ensures that the operation cannot begin until the image is available.
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkContext->imageAvailableSemaphores[vkContext->currentFrame];

    // Each semaphore waits on the corresponding pipeline stage to complete. 1:1 ratio.
    
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT prevents subsequent colour attachment
    // writes from executing until the semaphore signals (i.e. one frame is presented at a time)
    VkPipelineStageFlags flags[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.pWaitDstStageMask = flags;

    VkResult result = vkQueueSubmit(vkContext->device.graphicsQueue, 1, &submitInfo, 
        vkContext->inFlightFences[vkContext->currentFrame].handle);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("vkQueueSubmit failed with result: '%s'", VkResultMessage(result, true));
        return false;
    }

    NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(commandBuffer);

    // End queue submission
    // Give the image back to the swapchain.
    SwapChainPresent(vkContext, &vkContext->swapChain, vkContext->device.graphicsQueue,
        vkContext->device.presentQueue, vkContext->queueCompleteSemaphores[vkContext->currentFrame],
        vkContext->imageIndex);

    // TODO: Fix problem on class and ImGui
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

	return true;
}

bool VulkanBackend::RecreateResources()
{
    // If already being recreated, do not try again.
    if (vkContext->recreatingSwapchain)
    {
        NOUS_DEBUG("Recreate Swapchain called when already recreating. Booting.");
        return false;
    }

    // Detect if the window is too small to be drawn to.
    if (vkContext->framebufferWidth == 0 || vkContext->framebufferHeight == 0)
    {
        NOUS_DEBUG("Recreate Swapchain called when window is < 1 in a dimension. Booting.");
        return false;
    }

    // Mark as recreating if the dimensions are valid.
    vkContext->recreatingSwapchain = true;

    // Wait for any operations to complete.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // Clear these out just in case.
    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        vkContext->imagesInFlight[i] = 0;
    }

    // Requery support and depth format
    vkContext->device.swapChainSupport = QuerySwapChainSupport(vkContext->device.physicalDevice, vkContext);
    vkContext->device.depthFormat = FindDepthFormat(vkContext->device.physicalDevice);

    RecreateSwapChain(vkContext, cachedFramebufferWidth, cachedFramebufferHeight, &vkContext->swapChain);

    // Sync the framebuffer size with the cached sizes.
    vkContext->framebufferWidth = cachedFramebufferWidth;
    vkContext->framebufferHeight = cachedFramebufferHeight;

    vkContext->mainRenderpass.w = vkContext->framebufferWidth;
    vkContext->mainRenderpass.h = vkContext->framebufferHeight;

    cachedFramebufferWidth = 0;
    cachedFramebufferHeight = 0;

    // Update framebuffer size generation.
    vkContext->framebufferSizeLastGeneration = vkContext->framebufferSizeGeneration;

    // CleanUp swapchain.
    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.graphicsCommandPool, &vkContext->graphicsCommandBuffers[i]);
    }

    // Framebuffers.
    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        NOUS_VulkanFramebuffer::DestroyVulkanFramebuffer(vkContext, &vkContext->swapChain.swapChainFramebuffers[i]);
    }

    vkContext->mainRenderpass.x = 0;
    vkContext->mainRenderpass.y = 0;

    vkContext->mainRenderpass.w = vkContext->framebufferWidth;
    vkContext->mainRenderpass.h = vkContext->framebufferHeight;

    NOUS_VulkanFramebuffer::CreateFramebuffers(vkContext);

    NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext);

    // Clear the recreating flag.
    vkContext->recreatingSwapchain = false;

    return true;
}

void VulkanBackend::UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
    VulkanCommandBuffer* commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];

    UseMaterialShader(vkContext, &vkContext->materialShader);

    vkContext->materialShader.globalUBO.projection = projection;
    vkContext->materialShader.globalUBO.view = view;

    UpdateMaterialShaderGlobalState(vkContext, &vkContext->materialShader, vkContext->frameDeltaTime);
}

void VulkanBackend::UpdateObject(GeometryRenderData renderData)
{
    VulkanCommandBuffer* commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];

    UpdateMaterialShaderLocalState(vkContext, &vkContext->materialShader, renderData);

    // TODO: temporary test code

    UseMaterialShader(vkContext, &vkContext->materialShader);

    // Bind vertex buffer at offset.
    VulkanBuffer vertexBuffers[] = { vkContext->objectVertexBuffer };
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1, &vertexBuffers->handle, offsets);

    // Bind index buffer at offset.
    vkCmdBindIndexBuffer(commandBuffer->handle, vkContext->objectIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

    // Issue the draw.
    vkCmdDrawIndexed(commandBuffer->handle, myMesh->indices.size(), 1, 0, 0, 0);

    // TODO: end temporary test code
}

// ----------------------------------------------------------------------------------------------- //
// TEMPORAL //

void VulkanBackend::CreateTexture(const char* path, int32 width, int32 height, 
    int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture)
{
    outTexture->width = width;
    outTexture->height = height;
    outTexture->channelCount = channelCount;
    outTexture->generation = INVALID_ID;

    // Internal data creation.
    // TODO: Use an allocator for this.
    outTexture->internalData = reinterpret_cast<VulkanTextureData*>(
        MemoryManager::Allocate(sizeof(VulkanTextureData), MemoryManager::MemoryTag::TEXTURE));

    VulkanTextureData* textureData = (VulkanTextureData*)outTexture->internalData;
    VkDeviceSize imageSize = width * height * channelCount;

    // NOTE: Assumes 8 bits per channel.
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM; // RGBA

    // Create a staging buffer and load data into it.
    VkBufferUsageFlagBits usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VulkanBuffer stagingBuffer;
    NOUS_VulkanBuffer::CreateBuffer(vkContext, imageSize, usage, memoryPropertyFlags, true, &stagingBuffer);
    NOUS_VulkanBuffer::LoadBuffer(vkContext, &stagingBuffer, 0, imageSize, 0, pixels);

    // NOTE: Lots of assumptions here, different texture types will require
    // different options here.
    CreateVulkanImage(vkContext, VK_IMAGE_TYPE_2D, width, height, 1, VK_SAMPLE_COUNT_1_BIT, imageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_COLOR_BIT,
        &textureData->image);

    VulkanCommandBuffer tempCommandBuffer;
    VkCommandPool pool = vkContext->device.graphicsCommandPool;
    VkQueue queue = vkContext->device.graphicsQueue;

    NOUS_VulkanCommandBuffer::CommandBufferAllocateAndBeginSingleTime(vkContext, pool, &tempCommandBuffer);

    // Transition the layout from whatever it is currently to optimal for recieving data.
    TransitionVulkanImageLayout(vkContext, &tempCommandBuffer, &textureData->image, imageFormat,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the data from the buffer.
    CopyBufferToVulkanImage(vkContext, &textureData->image, stagingBuffer.handle, &tempCommandBuffer);

    // Transition from optimal for data reciept to shader-read-only optimal layout.
    TransitionVulkanImageLayout(vkContext, &tempCommandBuffer, &textureData->image, imageFormat,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    NOUS_VulkanCommandBuffer::CommandBufferEndAndFreeSingleTime(vkContext, pool, &tempCommandBuffer, queue);

    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &stagingBuffer);

    // Create a sampler for the texture
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // TODO: These filters should be configurable.
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16;

    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;

    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;

    VkResult result = vkCreateSampler(vkContext->device.logicalDevice, &samplerCreateInfo, vkContext->allocator, &textureData->sampler);
    
    if (!VkResultIsSuccess(result)) 
    {
        NOUS_ERROR("Error creating texture sampler: %s", VkResultMessage(result, true));
        return;
    }

    outTexture->hasTransparency = hasTransparency;
    outTexture->generation++;
}

void VulkanBackend::DestroyTexture(Texture* texture)
{
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    VulkanTextureData* textureData = reinterpret_cast<VulkanTextureData*>(texture->internalData);

    if (textureData) 
    {
        DestroyVulkanImage(vkContext, &textureData->image);
        MemoryManager::ZeroMemory(&textureData->image, sizeof(VulkanImage));

        vkDestroySampler(vkContext->device.logicalDevice, textureData->sampler, vkContext->allocator);
        textureData->sampler = 0;

        MemoryManager::Free(textureData, sizeof(VulkanTextureData), MemoryManager::MemoryTag::TEXTURE);
    }
    
    MemoryManager::ZeroMemory(texture, sizeof(Texture));
}

VulkanContext* VulkanBackend::GetVulkanContext()
{
    return vkContext;
}
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
#include "VulkanMaterialShader.h"
#include "VulkanUIShader.h"
#include "VulkanMultithreading.h"

#include "MaterialSystem.h"

#include "MemoryManager.h"
#include "Logger.h"

#include "ResourceMesh.h"
#include "ResourceTexture.h"
#include "ResourceMaterial.h"
#include "VulkanImGuiResources.h"

#include "NOUS_Thread.h"

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

    NOUS_ImGuiVulkanResources::CreateImGuiVulkanResources(vkContext);

    // Scene Render Pass
    NOUS_DEBUG("Creating Vulkan Scene Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateOffscreenRenderpass(vkContext, &vkContext->sceneRenderpass, 
        float4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
        float4(0.1f, 0.0f, 0.0f, 1.0f),
        1.0f,
        0, 
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER))
    {
        NOUS_ERROR("Failed to create Vulkan Scene Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Scene Render Pass created successfully!");
    }

    // Game Render Pass
    NOUS_DEBUG("Creating Vulkan Game Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateOffscreenRenderpass(vkContext, &vkContext->gameRenderpass,
        float4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
        float4(0.0f, 0.0f, 0.1f, 1.0f),
        1.0f,
        0,
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER))
    {
        NOUS_ERROR("Failed to create Vulkan Game Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Game Render Pass created successfully!");
    }

    // UI Render Pass
    NOUS_DEBUG("Creating Vulkan UI Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->uiRenderpass,
        float4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
        float4(0.0f, 0.0f, 0.0f, 0.0f),
        1.0f,
        0,
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER,
        false, false))
    {
        NOUS_ERROR("Failed to create Vulkan UI Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan UI Render Pass created successfully!");
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

    // MULTITHREADING
    // Create Vulkan Worker Command Pools
    NOUS_DEBUG("Creating Vulkan Worker Command Pools...");
    if (!NOUS_VulkanMultithreading::CreateWorkerCommandPools(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Worker Command Pools. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Worker Command Pools created successfully!");
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

#ifdef _DEBUG
    ExecuteBatchFile("compile-shaders.bat");
#endif // _DEBUG

    // Create Vulkan Material Shader
    NOUS_DEBUG("Creating Nous Material Shader...");
    if (!CreateMaterialShader(vkContext, &vkContext->sceneRenderpass, &vkContext->materialShader))
    {
        NOUS_ERROR("Failed to create Nous Material Shader. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Nous Material Shader created successfully!");
    }

    // Create Vulkan Game Shader
    NOUS_DEBUG("Creating Nous Game Shader...");
    if (!CreateMaterialShader(vkContext, &vkContext->gameRenderpass, &vkContext->gameShader))
    {
        NOUS_ERROR("Failed to create Nous Game Shader. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Nous Game Shader created successfully!");
    }

    // Create Vulkan UI Shader
    NOUS_DEBUG("Creating Nous UI Shader...");
    if (!CreateUIShader(vkContext, &vkContext->uiShader))
    {
        NOUS_ERROR("Failed to create Nous UI Shader. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Nous UI Shader created successfully!");
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

    // Mark all geometries as invalid
    for (uint32 i = 0; i < VULKAN_MAX_GEOMETRY_COUNT; ++i)
    {
        vkContext->geometries[i].ID = INVALID_ID;
        vkContext->geometries[i].generation = INVALID_ID;
    }

	return ret;
}

void VulkanBackend::Shutdown()
{
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    NOUS_VulkanBuffer::DestroyBuffers(vkContext);

    DestroyUIShader(vkContext, &vkContext->uiShader);
    DestroyMaterialShader(vkContext, &vkContext->materialShader);
    DestroyMaterialShader(vkContext, &vkContext->gameShader);

    NOUS_VulkanSyncObjects::DestroySyncObjects(vkContext);

    NOUS_VulkanMultithreading::DestroyWorkerCommandPools(vkContext);

    NOUS_VulkanCommandBuffer::DestroyCommandBuffers(vkContext);

    NOUS_VulkanFramebuffer::DestroyFramebuffers(vkContext);

    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->uiRenderpass);
    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->gameRenderpass);
    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->sceneRenderpass);

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
    ProcessPendingSubmissions();

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
    VkResult result = vkWaitForFences(vkContext->device.logicalDevice, 1, &vkContext->inFlightFences[vkContext->currentFrame], true, UINT64_MAX);
    if (!VkResultIsSuccess(result))
    {
        NOUS_FATAL("In-flight fence wait failure! Error: %s", VkResultMessage(result, true).c_str());
        return false;
    }

    // Acquire the next image from the swap chain. Pass along the semaphore that should signaled when this completes.
    // This same semaphore will later be waited on by the queue submission to ensure this image is available.
    if (!SwapChainAcquireNextImageIndex(vkContext, &vkContext->swapChain, UINT64_MAX,
        vkContext->imageAvailableSemaphores[vkContext->currentFrame], 0, &vkContext->imageIndex))
    {
        NOUS_FATAL("Failed to acquire next image index, booting.");
        return false;
    }

    return true;
}

bool VulkanBackend::EndFrame(float dt)
{
    // Make sure the previous frame is not using this image (i.e. its fence is being waited on)
    if (vkContext->imagesInFlight[vkContext->imageIndex] != VK_NULL_HANDLE) // was frame
    {  
        VkResult result = vkWaitForFences(vkContext->device.logicalDevice, 1, &vkContext->imagesInFlight[vkContext->imageIndex], true, UINT64_MAX);
        if (!VkResultIsSuccess(result))
        {
            NOUS_FATAL("VkFence wait failure! Error: %s", VkResultMessage(result, true));
            return false;
        }
    }

    // Mark the image fence as in-use by this frame.
    vkContext->imagesInFlight[vkContext->imageIndex] = vkContext->inFlightFences[vkContext->currentFrame];

    // Reset the fence for use on the next frame
    VK_CHECK(vkResetFences(vkContext->device.logicalDevice, 1, &vkContext->inFlightFences[vkContext->currentFrame]));

    std::array<VulkanCommandBuffer*, 3> commandBuffers = { 
        &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex], 
        &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex], 
        &vkContext->graphicsCommandBuffers[vkContext->imageIndex] 
    };

    std::array<VkCommandBuffer, 3> commandBuffersPtrs = { 
        vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex].handle,
        vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex].handle,
        vkContext->graphicsCommandBuffers[vkContext->imageIndex].handle 
    };

    // Submit the queue and wait for the operation to complete.
    // Begin queue submission
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Command buffer(s) to be executed.
    submitInfo.commandBufferCount = static_cast<uint32>(commandBuffersPtrs.size());
    submitInfo.pCommandBuffers = commandBuffersPtrs.data();

    // The semaphore(s) to be signaled when the queue is complete.
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vkContext->queueCompleteSemaphores[vkContext->currentFrame];

    // Wait semaphore ensures that the operation cannot begin until the image is available.
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkContext->imageAvailableSemaphores[vkContext->currentFrame];

    // Each semaphore waits on the corresponding pipeline stage to complete. 1:1 ratio.
    
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT prevents subsequent colour attachment
    // writes from executing until the semaphore signals (i.e. one frame is presented at a time)
    VkPipelineStageFlags flags[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT };
    submitInfo.pWaitDstStageMask = flags;

    VkResult result = vkQueueSubmit(vkContext->device.graphicsQueue, 1, &submitInfo, 
        vkContext->inFlightFences[vkContext->currentFrame]);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("vkQueueSubmit failed with result: '%s'", VkResultMessage(result, true));
        return false;
    }

    for (auto& cb : commandBuffers) 
    {
        NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(cb);
    }

    // End queue submission
    // Give the image back to the swapchain.
    SwapChainPresent(vkContext, &vkContext->swapChain, vkContext->device.graphicsQueue,
        vkContext->device.presentQueue, vkContext->queueCompleteSemaphores[vkContext->currentFrame],
        vkContext->imageIndex);

    // TODO: Fix problem on class and ImGui
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

	return true;
}

bool VulkanBackend::BeginRenderpass(BuiltInRenderpass renderpassID)
{
    // Begin recording commands.
    VulkanCommandBuffer* commandBuffer = nullptr;
    VulkanRenderpass* renderpass = nullptr;
    VkFramebuffer framebuffer = 0;
    
    switch (renderpassID)
    {
        case BuiltInRenderpass::SCENE: 
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->sceneRenderpass;
            framebuffer = vkContext->imGuiResources.m_ViewportFramebuffers[vkContext->imageIndex];
            break;
        }
        case BuiltInRenderpass::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->gameRenderpass;
            framebuffer = vkContext->imGuiResources.m_GameViewportFramebuffers[vkContext->imageIndex];
            break;
        }
        case BuiltInRenderpass::UI:
        {
            commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->uiRenderpass;
            framebuffer = vkContext->swapChain.swapChainFramebuffers[vkContext->imageIndex];
            break;
        }
        default:
        {
            NOUS_ERROR("Vulkan Renderpass called on an unrecognized renderpass ID.");
            return false;
        }
    }

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

    vkContext->sceneRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->sceneRenderpass.renderArea.w = vkContext->framebufferHeight;

    vkContext->gameRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->gameRenderpass.renderArea.w = vkContext->framebufferHeight;

    vkContext->uiRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->uiRenderpass.renderArea.w = vkContext->framebufferHeight;

    NOUS_VulkanRenderpass::BeginRenderpass(commandBuffer, renderpass, framebuffer);

    switch (renderpassID)
    {
        case BuiltInRenderpass::SCENE:
        {
            UseMaterialShader(vkContext, commandBuffer, &vkContext->materialShader);
            break;
        }
        case BuiltInRenderpass::GAME:
        {
            UseMaterialShader(vkContext, commandBuffer, &vkContext->gameShader);
            break;
        }
        case BuiltInRenderpass::UI:
        {
            UseUIShader(vkContext, &vkContext->uiShader);
            break;
        }
    }

    return true;
}

bool VulkanBackend::EndRenderpass(BuiltInRenderpass renderpassID)
{
    VulkanRenderpass* renderpass = nullptr;
    VulkanCommandBuffer* commandBuffer = nullptr;
    
    switch (renderpassID)
    {
        case BuiltInRenderpass::SCENE:
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->sceneRenderpass;
            break;
        }
        case BuiltInRenderpass::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->gameRenderpass;
            break;
        }
        case BuiltInRenderpass::UI:
        {
            commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->uiRenderpass;
            break;
        }
        default:
        {
            NOUS_ERROR("Vulkan Renderpass called on an unrecognized renderpass ID.");
            return false;
        }
    }

    // End renderpass
    NOUS_VulkanRenderpass::EndRenderpass(commandBuffer, renderpass);

    // End command buffer
    NOUS_VulkanCommandBuffer::CommandBufferEnd(commandBuffer);

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

    NOUS_ImGuiVulkanResources::RecreateImGuiVulkanResources(vkContext);

    vkContext->sceneRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->sceneRenderpass.renderArea.w = vkContext->framebufferHeight;

    vkContext->gameRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->gameRenderpass.renderArea.w = vkContext->framebufferHeight;

    vkContext->uiRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->uiRenderpass.renderArea.w = vkContext->framebufferHeight;

    cachedFramebufferWidth = 0;
    cachedFramebufferHeight = 0;

    // Update framebuffer size generation.
    vkContext->framebufferSizeLastGeneration = vkContext->framebufferSizeGeneration;

    // CleanUp swapchain.
    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &vkContext->graphicsCommandBuffers[i]);
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &vkContext->imGuiResources.m_ViewportCommandBuffers[i]);
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &vkContext->imGuiResources.m_GameViewportCommandBuffers[i]);
    }

    // Framebuffers.
    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportFramebuffers[i], vkContext->allocator);
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_GameViewportFramebuffers[i], vkContext->allocator);
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->swapChain.swapChainFramebuffers[i], vkContext->allocator);
    }

    vkContext->sceneRenderpass.renderArea.x = 0;
    vkContext->sceneRenderpass.renderArea.y = 0;

    vkContext->sceneRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->sceneRenderpass.renderArea.w = vkContext->framebufferHeight;

    vkContext->gameRenderpass.renderArea.x = 0;
    vkContext->gameRenderpass.renderArea.y = 0;

    vkContext->gameRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->gameRenderpass.renderArea.w = vkContext->framebufferHeight;

    vkContext->uiRenderpass.renderArea.x = 0;
    vkContext->uiRenderpass.renderArea.y = 0;

    vkContext->uiRenderpass.renderArea.z = vkContext->framebufferWidth;
    vkContext->uiRenderpass.renderArea.w = vkContext->framebufferHeight;

    // Regenerate world framebuffers
    NOUS_VulkanFramebuffer::CreateFramebuffers(vkContext);

    NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext);

    // Clear the recreating flag.
    vkContext->recreatingSwapchain = false;

    return true;
}

void VulkanBackend::UpdateGlobalWorldState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
    VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);

    VulkanMaterialShader* shader = nullptr;

    if (renderpassID == BuiltInRenderpass::GAME) 
    {
        shader = &vkContext->gameShader;
    }
    else 
    {
        shader = &vkContext->materialShader;
    }

    UseMaterialShader(vkContext, commandBuffer, shader);

    shader->globalUBO.projection = projection;
    shader->globalUBO.view = view;

    UpdateMaterialShaderGlobalState(vkContext, commandBuffer, shader, vkContext->frameDeltaTime);
}

void VulkanBackend::UpdateGlobalUIState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, int32 mode)
{
    UseUIShader(vkContext, &vkContext->uiShader);

    vkContext->uiShader.globalUBO.projection = projection;
    vkContext->uiShader.globalUBO.view = view;

    UpdateUIShaderGlobalState(vkContext, &vkContext->uiShader, vkContext->frameDeltaTime);
}

VulkanCommandBuffer* VulkanBackend::GetCommandBufferByRenderpassID(BuiltInRenderpass renderpassID) 
{
    VulkanCommandBuffer* commandBuffer = nullptr;

    switch (renderpassID)
    {
        case BuiltInRenderpass::SCENE:
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            break;
        }
        case BuiltInRenderpass::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            break;
        }
        case BuiltInRenderpass::UI:
        {
            commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];
            break;
        }
        default:
        {
            NOUS_ERROR("Vulkan Renderpass called on an unrecognized renderpass ID.");
        }
    }

    return commandBuffer;
}

void VulkanBackend::DrawGeometry(BuiltInRenderpass renderpassID, GeometryRenderData renderData)
{
    // Ignore non-uploaded geometries.
    if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID)
    {
        return;
    }

    VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);

    VulkanMaterialShader* shader = nullptr;

    if (renderpassID == BuiltInRenderpass::GAME)
    {
        shader = &vkContext->gameShader;
    }
    else
    {
        shader = &vkContext->materialShader;
    }

    VulkanGeometryData* bufferData = &vkContext->geometries[renderData.geometry->internalID];

    UseMaterialShader(vkContext, commandBuffer, shader);

    MaterialShaderSetModel(vkContext, commandBuffer, shader, renderData.model);

    ResourceMaterial* material = nullptr;

    if (renderData.geometry->material) 
    {
        material = renderData.geometry->material;
    }
    else 
    {
        material = NOUS_MaterialSystem::GetDefaultMaterial();
    }

    MaterialShaderApplyMaterial(vkContext, commandBuffer, shader, material);

    // Bind vertex buffer at offset.
    VkDeviceSize offsets[1] = { bufferData->vertexBufferOffset };

    vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1, &vkContext->objectVertexBuffer.handle, (VkDeviceSize*)offsets);

    // Draw indexed or non-indexed.
    if (bufferData->indexCount > 0) 
    {
        // Bind index buffer at offset.
        vkCmdBindIndexBuffer(commandBuffer->handle, vkContext->objectIndexBuffer.handle, bufferData->indexBufferOffset, VK_INDEX_TYPE_UINT32);
        // Issue the draw.
        vkCmdDrawIndexed(commandBuffer->handle, bufferData->indexCount, 1, 0, 0, 0);
    }
    else 
    {
        vkCmdDraw(commandBuffer->handle, bufferData->vertexCount, 1, 0, 0);
    }
}

// ----------------------------------------------------------------------------------------------- //
// TEMPORAL //

void VulkanBackend::CreateTexture(const uint8* pixels, ResourceTexture* texture)
{
    // Internal data creation.
    // TODO: Use an allocator for this.
    texture->internalData = reinterpret_cast<VulkanTextureData*>(
        MemoryManager::Allocate(sizeof(VulkanTextureData), MemoryManager::MemoryTag::TEXTURE));

    VulkanTextureData* textureData = (VulkanTextureData*)texture->internalData;
    VkDeviceSize imageSize = texture->width * texture->height * texture->channelCount;

    // NOTE: Assumes 8 bits per channel.
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM; // RGBA

    // Create a staging buffer and load data into it.
    VkBufferUsageFlagBits usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VulkanBuffer stagingBuffer;

    NOUS_VulkanBuffer::CreateBuffer(vkContext, imageSize, usage, memoryPropertyFlags, true, &stagingBuffer);
    NOUS_VulkanBuffer::LoadData(vkContext, &stagingBuffer, 0, imageSize, 0, pixels);

    // NOTE: Lots of assumptions here, different texture types will require
    // different options here.
    CreateVulkanImage(vkContext, VK_IMAGE_TYPE_2D, texture->width, texture->height, 1, VK_SAMPLE_COUNT_1_BIT, imageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_COLOR_BIT,
        &textureData->image);

    VulkanCommandBuffer tempCommandBuffer;
    VkCommandPool pool = NOUS_VulkanMultithreading::GetThreadCommandPool(vkContext, NOUS_Multithreading::NOUS_Thread::GetThreadID(std::this_thread::get_id()));
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
}

void VulkanBackend::DestroyTexture(ResourceTexture* texture)
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
}

bool VulkanBackend::CreateMaterial(ResourceMaterial* material)
{
    if (material) 
    {
        if (!AcquireMaterialShaderResources(vkContext, &vkContext->materialShader, material))
        {
            NOUS_ERROR("VulkanBackend::CreateMaterial() - Failed to acquire shader resources.");
            return false;
        }

        if (!AcquireMaterialShaderResources(vkContext, &vkContext->gameShader, material))
        {
            NOUS_ERROR("VulkanBackend::CreateMaterial() - Failed to acquire shader resources.");
            return false;
        }

        NOUS_TRACE("Renderer: Material created.");
        return true;
    }

    NOUS_ERROR("VulkanBackend::CreateMaterial() called with nullptr. Creation failed.");
    return false;
}

void VulkanBackend::DestroyMaterial(ResourceMaterial* material)
{
    if (material) 
    {
        if (material->internalID != INVALID_ID) 
        {
            ReleaseMaterialShaderResources(vkContext, &vkContext->materialShader, material);
            //ReleaseMaterialShaderResources(vkContext, &vkContext->gameShader, material);
        }
        else 
        {
            NOUS_WARN("VulkanBackend::DestroyMaterial() called with internal_id = INVALID_ID. Nothing was done.");
        }
    }
    else 
    {
        NOUS_WARN("VulkanBackend::DestroyMaterial() called with nullptr. Nothing was done.");
    }
}

bool VulkanBackend::CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* geometry)
{
    if (!vertexCount || !vertices) 
    {
        NOUS_ERROR("VulkanBackend::CreateGeometry() requires vertex data, and none was supplied. vertexCount=%d, vertices=%p", vertexCount, vertices);
        return false;
    }

    // Check if this is a re-upload. If it is, need to free old data afterward.
    // TODO: Resource Manager needs to take care of reuploads, not here.
    
    //bool isReupload = false;
    bool isReupload = geometry->internalID != INVALID_ID;

    VulkanGeometryData oldRange;
    VulkanGeometryData* internalData = nullptr;

    if (isReupload)
    {
        internalData = &vkContext->geometries[geometry->internalID];

        // Take a copy of the old range.
        oldRange.indexBufferOffset = internalData->indexBufferOffset;
        oldRange.indexCount = internalData->indexCount;
        oldRange.indexSize = internalData->indexSize;

        oldRange.vertexBufferOffset = internalData->vertexBufferOffset;
        oldRange.vertexCount = internalData->vertexCount;
        oldRange.vertexSize = internalData->vertexSize;
    }
    else
    {
        for (u32 i = 0; i < VULKAN_MAX_GEOMETRY_COUNT; ++i)
        {
            if (vkContext->geometries[i].ID == INVALID_ID)
            {
                // Found a free index.
                geometry->internalID = i;
                vkContext->geometries[i].ID = i;
                internalData = &vkContext->geometries[i];
                break;
            }
        }
    }

    if (!internalData) 
    {
        NOUS_FATAL("VulkanBackend::CreateGeometry() failed to find a free index for a new geometry upload. Adjust config to allow for more.");
        return false;
    }

    VkCommandPool pool = NOUS_VulkanMultithreading::GetThreadCommandPool(vkContext, NOUS_Multithreading::NOUS_Thread::GetThreadID(std::this_thread::get_id()));
    VkQueue queue = vkContext->device.graphicsQueue;

    // Vertex data.
    internalData->vertexCount = vertexCount;
    internalData->vertexSize = sizeof(Vertex3D) * vertexCount;

    if (!NOUS_VulkanBuffer::UploadDataRange(vkContext, pool, 0, queue, &vkContext->objectVertexBuffer,
        &internalData->vertexBufferOffset, internalData->vertexSize, vertices))
    {
        NOUS_ERROR("VulkanBackend::CreateGeometry() failed to upload to the vertex buffer!");
        return false;
    }

    // Index data, if applicable
    if (indexCount && indices)
    {
        internalData->indexCount = indexCount;
        internalData->indexSize = sizeof(uint32) * indexCount;

        if (!NOUS_VulkanBuffer::UploadDataRange(vkContext, pool, 0, queue, &vkContext->objectIndexBuffer,
            &internalData->indexBufferOffset, internalData->indexSize, indices))
        {
            NOUS_ERROR("VulkanBackend::CreateGeometry() failed to upload to the index buffer!");
            return false;
        }
    }

    if (internalData->generation == INVALID_ID) 
    {
        internalData->generation = 0;
    }
    else 
    {
        internalData->generation++;
    }

    if (isReupload) 
    {
        // Free vertex data
        NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectVertexBuffer, 
            oldRange.vertexBufferOffset, oldRange.vertexSize);

        // Free index data, if applicable
        if (oldRange.indexSize > 0) 
        {
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectIndexBuffer, 
                oldRange.indexBufferOffset, oldRange.indexSize);
        }
    }

    return true;
}

void VulkanBackend::DestroyGeometry(ResourceMesh* geometry)
{
    if (geometry && geometry->internalID != INVALID_ID) 
    {
        vkDeviceWaitIdle(vkContext->device.logicalDevice);

        VulkanGeometryData* internalData = &vkContext->geometries[geometry->internalID];

        // Free vertex data
        NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectVertexBuffer, internalData->vertexBufferOffset, internalData->vertexSize);

        // Free index data, if applicable
        if (internalData->indexSize > 0) 
        {
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectIndexBuffer, internalData->indexBufferOffset, internalData->indexSize);
        }

        // Clean up data.
        MemoryManager::ZeroMemory(internalData, sizeof(VulkanGeometryData));

        internalData->ID = INVALID_ID;
        internalData->generation = INVALID_ID;
    }
}

VulkanContext* VulkanBackend::GetVulkanContext()
{
    return vkContext;
}

void VulkanBackend::ProcessPendingSubmissions() 
{
    std::unique_lock<std::mutex> lock(vkContext->submitQueueMutex);

    while (!vkContext->submitQueue.empty()) 
    {
        auto task = std::move(vkContext->submitQueue.front());
        vkContext->submitQueue.pop_front();

        lock.unlock(); // Unlock while processing

        std::lock_guard<std::mutex> queueLock(vkContext->device.graphicsQueueMutex);
        VkResult result = vkQueueSubmit(task.queue, task.submitCount, task.pSubmits, task.fence);

        bool success = (result == VK_SUCCESS);
        if (success && task.waitIdle) 
        {
            success = (vkQueueWaitIdle(task.queue) == VK_SUCCESS);
        }

        task.resultPromise.set_value(success);

        lock.lock(); // Re-lock for next iteration
    }
}
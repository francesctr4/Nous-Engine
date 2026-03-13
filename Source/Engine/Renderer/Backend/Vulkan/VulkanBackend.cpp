#include <Engine/Renderer/Backend/Vulkan/VulkanBackend.h>
#include <Engine/Renderer/Backend/Vulkan/VulkanTypes.inl>

#include "Engine/Renderer/Backend/Vulkan/Core/Device/VulkanDevice.h"
#include "Engine/Renderer/Backend/Vulkan/Core/Swapchain/VulkanSwapchain.h"
#include "Engine/Renderer/Backend/Vulkan/Rendering/Renderpass/VulkanRenderpass.h"
#include "Engine/Renderer/Backend/Vulkan/Rendering/CommandBuffer/VulkanCommandBuffer.h"
#include "Engine/Renderer/Backend/Vulkan/Rendering/Framebuffer/VulkanFramebuffer.h"
#include "Engine/Renderer/Backend/Vulkan/Rendering/SyncObjects/VulkanSyncObjects.h"
#include "Engine/Renderer/Backend/Vulkan/Utils/VulkanUtils.h"
#include "Engine/Renderer/Backend/Vulkan/Core/DebugMessenger/VulkanDebugMessenger.h"
#include "Engine/Renderer/Backend/Vulkan/Core/Instance/VulkanInstance.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/Buffer/VulkanBuffer.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/Image/VulkanImage.h"
#include "Engine/Renderer/Backend/Vulkan/Rendering/CommandBuffer/VulkanMultithreading.h"

#include <glm/gtc/matrix_transform.hpp>

#include <Engine/Core/FileSystem/FileSystem.h>

#include <Engine/Core/MemoryManager/MemoryManager.h>
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/Shader/VulkanShader.h"

#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include <Engine/Core/Application.h>
#include <Engine/Core/EventSystem/EventSystem.h>
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_BACKEND_VULKAN_BACKEND;

VulkanContext* VulkanBackend::vkContext = nullptr;

VulkanBackend::VulkanBackend()
{
    vkContext = NOUS_NEW<VulkanContext>(MemoryTag::RENDERER);
}

VulkanBackend::~VulkanBackend()
{
    NOUS_DELETE(vkContext, MemoryTag::RENDERER);
}

bool VulkanBackend::Initialize()
{
    bool ret = true;

    NOUS_INFO_C(CURRENT_CHANNEL, " ----------------------- USING VULKAN BACKEND ----------------------- ");

    // TODO: Custom allocator
    vkContext->allocator = 0;

    // Get Framebuffer Size
    External->window->GetFramebufferSize(&cachedFramebufferWidth, &cachedFramebufferHeight);

    vkContext->framebufferWidth = (cachedFramebufferWidth != 0) ? cachedFramebufferWidth : WINDOW_WIDTH;
    vkContext->framebufferHeight = (cachedFramebufferHeight != 0) ? cachedFramebufferHeight : WINDOW_HEIGHT;

    cachedFramebufferWidth = 0;
    cachedFramebufferWidth = 0;

    // Instance
    NOUS_DEBUG_C(CURRENT_CHANNEL, "[%s][VULKAN] Creating Vulkan instance...", __FUNCTION__);
    if (!NOUS_VulkanInstance::CreateInstance(vkContext)) 
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Instance. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Instance created successfully!");
    }

    // Debugger
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Debugger...");
    if (!NOUS_VulkanDebugMessenger::SetupDebugMessenger(vkContext)) 
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Debugger. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Debugger created successfully!");
    }

    // Surface
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan surface...");
    if (!NOUS_VulkanInstance::CreateSurface(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Surface. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Surface created successfully!");
    }

    // Physical Device
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Searching for a suitable Physical Device...");
    if (!NOUS_VulkanDevice::PickPhysicalDevice(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to find a suitable GPU!");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Suitable GPU found!");
    }

    // Logical Device
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Logical Device...");
    if (!NOUS_VulkanDevice::CreateLogicalDevice(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Logical Device. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Logical Device created successfully!");
    }

    // Swap Chain
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Swap Chain...");
    if (!NOUS_VulkanSwapChain::CreateSwapChain(vkContext, vkContext->framebufferWidth, vkContext->framebufferHeight, &vkContext->swapChain))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Swap Chain. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Swap Chain created successfully!");
    }

    NOUS_ImGuiVulkanResources::CreateImGuiVulkanResources(vkContext);

    // Scene Render Pass
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Scene Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->sceneRenderpass,
        glm::vec4(0.0f, 0.0f, vkContext->framebufferWidth, vkContext->framebufferHeight),
        glm::vec4(0.1f, 0.0f, 0.0f, 1.0f),
        1.0f, 0,
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER,
        false, false, true))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Scene Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Scene Render Pass created successfully!");
    }

    // Pick Render Pass (R8G8B8A8_UNORM — no sRGB gamma, preserves raw ID bytes)
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Pick Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->pickRenderpass,
        glm::vec4(0.0f, 0.0f, vkContext->framebufferWidth, vkContext->framebufferHeight),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
        1.0f, 0,
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER,
        false, false, true,
        VK_FORMAT_R8G8B8A8_UNORM))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Pick Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Pick Render Pass created successfully!");
    }

    // Game Render Pass
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Game Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->gameRenderpass,
        glm::vec4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
        glm::vec4(0.0f, 0.0f, 0.1f, 1.0f),
        1.0f, 0,
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER,
        false, false, true))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Game Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Game Render Pass created successfully!");
    }

    // UI Render Pass
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan UI Render Pass...");
    if (!NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->uiRenderpass,
        glm::vec4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
        1.0f, 0,
        RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER,
        false, false, false))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan UI Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan UI Render Pass created successfully!");
    }

    // Swapchain Framebuffers
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Swapchain Framebuffers...");
    if (!NOUS_VulkanFramebuffer::CreateFramebuffers(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Swapchain Framebuffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Swapchain Framebuffers created successfully!");
    }

    // Create Command Buffers
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Command Buffers...");
    if (!NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Command Buffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Command Buffers created successfully!");
    }

    // MULTITHREADING
    // Create Vulkan Worker Command Pools
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Worker Command Pools...");
    if (!NOUS_VulkanMultithreading::CreateWorkerCommandPools(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Worker Command Pools. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Worker Command Pools created successfully!");
    }

    // Create Sync Objects
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Sync Objects...");
    if (!NOUS_VulkanSyncObjects::CreateSyncObjects(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Sync Objects. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Sync Objects created successfully!");
    }

    // BuiltIn shaders are loaded via the ResourceManager in ModuleRenderer3D::Awake(),
    // which calls ImporterShader::Load → CreateShader → VulkanBackend::CreateShader.
    // vkContext->builtInMaterialShader and builtInGameShader are assigned automatically
    // when the built-in asset path is recognised there.

    // Create Vulkan Buffers
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Buffers...");
    if (!NOUS_VulkanBuffer::CreateBuffers(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Buffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Buffers created successfully!");
    }

    // Mark all geometries as invalid
    for (uint32 i = 0; i < VULKAN_MAX_GEOMETRY_COUNT; ++i)
    {
        vkContext->geometries[i].ID = INVALID_ID;
        vkContext->geometries[i].generation = INVALID_ID;
    }

	return ret;
}

void VulkanBackend::Shutdown() noexcept
{
    // Ensure frame resources are released before tearing down infrastructure.
    // No-op if ReleaseFrameResources() was already called by the Editor or Renderer.
    ReleaseFrameResources();

    NOUS_VulkanBuffer::DestroyBuffers(vkContext);

    // builtInMaterialShader is managed by the ResourceManager;
    // ClearResources() → DestroyShader() releases its GPU resources and nulls this
    // pointer before Shutdown() is called. Guard against the unexpected case.
    if (vkContext->builtInMaterialShader)
        NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInMaterialShader pointer still set — ResourceManager may not have cleared resources.");
    vkContext->builtInMaterialShader = nullptr;

    // builtInGameShader is an internal clone (same SPIR-V, game renderpass) that is
    // NOT tracked by the ResourceManager, so we own it here.
    if (vkContext->builtInGameShader)
    {
        if (vkContext->builtInGameShader->internalData)
        {
            vkContext->builtInGameShader->internalData->Destroy();
            vkContext->builtInGameShader->internalData = nullptr;
        }
        NOUS_DELETE(vkContext->builtInGameShader, MemoryTag::RESOURCE_SHADER);
        vkContext->builtInGameShader = nullptr;
    }

    // builtInPickShader is an internal clone for mouse picking, also owned by VulkanBackend.
    if (vkContext->builtInPickShader)
    {
        if (vkContext->builtInPickShader->internalData)
        {
            vkContext->builtInPickShader->internalData->Destroy();
            vkContext->builtInPickShader->internalData = nullptr;
        }
        NOUS_DELETE(vkContext->builtInPickShader, MemoryTag::RESOURCE_SHADER);
        vkContext->builtInPickShader = nullptr;
    }

    // builtInOutlineShader is managed by the ResourceManager (like builtInMaterialShader).
    // ClearResources() → DestroyShader() releases its GPU resources before Shutdown().
    if (vkContext->builtInOutlineShader)
        NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInOutlineShader pointer still set — ResourceManager may not have cleared resources.");
    vkContext->builtInOutlineShader = nullptr;

    NOUS_VulkanSyncObjects::DestroySyncObjects(vkContext);

    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->uiRenderpass);
    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->gameRenderpass);
    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->pickRenderpass);
    NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->sceneRenderpass);

    NOUS_VulkanSwapChain::DestroySwapChain(vkContext, &vkContext->swapChain);

    NOUS_VulkanDevice::DestroyLogicalDevice(vkContext);

    NOUS_VulkanInstance::DestroySurface(vkContext);

    NOUS_VulkanDebugMessenger::DestroyDebugUtilsMessengerEXT(vkContext->instance, vkContext->debugMessenger, vkContext->allocator);

    NOUS_VulkanInstance::DestroyInstance(vkContext);
}

void VulkanBackend::ReleaseFrameResources() noexcept
{
    if (m_frameResourcesReleased) return;

    // Wait for the GPU to finish all submitted work.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // Free command buffers so they no longer reference pipelines, descriptor sets,
    // and vertex/index buffers.  Without this, the validation layer reports lifetime
    // violations (e.g. VUID-vkDestroyBuffer-buffer-00922) even though the GPU has
    // finished, because Vulkan tracks CPU-side object references.
    NOUS_VulkanMultithreading::DestroyWorkerCommandPools(vkContext);
    NOUS_VulkanCommandBuffer::DestroyCommandBuffers(vkContext);

    // Destroy framebuffers so they no longer reference the offscreen texture imageViews.
    // DestroyTexture() would otherwise trigger VUID-vkDestroyImageView-imageView-01026
    // "in use by VkFramebuffer".
    NOUS_VulkanFramebuffer::DestroyFramebuffers(vkContext);

    m_frameResourcesReleased = true;
}

void VulkanBackend::Resized(uint16 width, uint16 height) noexcept
{
    // Update the "framebuffer size generation", a counter which indicates when the
    // framebuffer size has been updated.

    cachedFramebufferWidth = width;
    cachedFramebufferHeight = height;

    vkContext->framebufferSizeGeneration++;

    NOUS_INFO_C(CURRENT_CHANNEL, "Vulkan Renderer Backend --> Resized: W / H / GEN: %i / %i / %llu", width, height, vkContext->framebufferSizeGeneration);
}

FrameResult VulkanBackend::BeginFrame(float dt)
{
    ProcessPendingSubmissions();

    vkContext->frameDeltaTime = dt;
    VulkanDevice* device = &vkContext->device;

    // If we are in the middle of recreating the swapchain, attempt to rebuild now.
    if (vkContext->recreatingSwapchain)
    {
        // Clear the flag BEFORE calling RecreateResources() — otherwise RecreateResources()
        // would immediately return false seeing the flag still set.
        vkContext->recreatingSwapchain = false;

        VkResult waitRes = vkDeviceWaitIdle(device->logicalDevice);
        if (!VkResultIsSuccess(waitRes))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::BeginFrame() --> vkDeviceWaitIdle (recreate) failed: '%s'",
                       VkResultMessage(waitRes, true).c_str());
            return FrameResult::ERROR;
        }

        // Try to rebuild swapchain-dependent resources. If this fails, skip again (e.g., minimized).
        if (!RecreateResources())
        {
            NOUS_INFO_C(CURRENT_CHANNEL, "Swapchain recreation still pending (likely minimized). Skipping frame.");
            return FrameResult::SKIPPED;
        }

        NOUS_INFO_C(CURRENT_CHANNEL, "Swapchain recreated. Skipping frame to complete transition.");
        return FrameResult::SKIPPED;
    }

    // Detect resize: framebuffer generation changed -> rebuild swapchain
    if (vkContext->framebufferSizeGeneration != vkContext->framebufferSizeLastGeneration)
    {
        VkResult waitRes = vkDeviceWaitIdle(device->logicalDevice);
        if (!VkResultIsSuccess(waitRes))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::BeginFrame() --> vkDeviceWaitIdle (resize) failed: '%s'",
                       VkResultMessage(waitRes, true).c_str());
            return FrameResult::ERROR;
        }

        if (!RecreateResources())
        {
            NOUS_INFO_C(CURRENT_CHANNEL, "Resize detected but resources not ready (likely minimized). Skipping frame.");
            return FrameResult::SKIPPED;
        }

        NOUS_INFO_C(CURRENT_CHANNEL, "Resize handled. Skipping this frame.");
        return FrameResult::SKIPPED;
    }

    // Wait for the current frame's fence (CPU/GPU sync).
    {
        VkFence inFlight = vkContext->inFlightFences[vkContext->currentFrame];
        VkResult fenceRes = vkWaitForFences(device->logicalDevice, 1, &inFlight, VK_TRUE, UINT64_MAX);
        if (!VkResultIsSuccess(fenceRes))
        {
            NOUS_FATAL_C(CURRENT_CHANNEL, "In-flight fence wait failure! Error: %s", VkResultMessage(fenceRes, true).c_str());
            return FrameResult::ERROR;
        }
    }

    // Acquire next image from the swapchain.
    // IMPORTANT: handle OUT_OF_DATE and SUBOPTIMAL as "skip and recreate".
    {
        VkResult acquireRes = NOUS_VulkanSwapChain::SwapChainAcquireNextImageIndex(
                vkContext,
                &vkContext->swapChain,
                UINT64_MAX,
                vkContext->imageAvailableSemaphores[vkContext->currentFrame],
                VK_NULL_HANDLE,
                &vkContext->imageIndex);

        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR)
        {
            // Trigger recreation on next frame; skip now.
            vkContext->recreatingSwapchain = true;
            NOUS_INFO_C(CURRENT_CHANNEL, "Swapchain acquire returned %s. Scheduling recreation, skipping frame.",
                      VkResultMessage(acquireRes, true).c_str());
            return FrameResult::SKIPPED;
        }

        if (!VkResultIsSuccess(acquireRes))
        {
            NOUS_FATAL_C(CURRENT_CHANNEL, "Failed to acquire next image index: %s", VkResultMessage(acquireRes, true).c_str());
            return FrameResult::ERROR;
        }
    }

    // Wait for any previous frame that was rendering to this swapchain image to finish.
    // Must happen here — BEFORE BeginRenderpass resets/records the command buffer for imageIndex.
    if (vkContext->imagesInFlight[vkContext->imageIndex] != VK_NULL_HANDLE)
    {
        VkFence imgFence = vkContext->imagesInFlight[vkContext->imageIndex];
        VkResult waitRes = vkWaitForFences(device->logicalDevice, 1, &imgFence, VK_TRUE, UINT64_MAX);
        if (!VkResultIsSuccess(waitRes))
        {
            NOUS_FATAL_C(CURRENT_CHANNEL, "Image fence wait failure! Error: %s", VkResultMessage(waitRes, true).c_str());
            return FrameResult::ERROR;
        }
    }

    // Mark this swapchain image as now being used by this frame's fence.
    vkContext->imagesInFlight[vkContext->imageIndex] = vkContext->inFlightFences[vkContext->currentFrame];

    return FrameResult::SUCCESS;
}

FrameResult VulkanBackend::EndFrame(float /*dt*/)
{
    // Reset the current frame fence for reuse.
    {
        VkResult resetRes = vkResetFences(vkContext->device.logicalDevice, 1, &vkContext->inFlightFences[vkContext->currentFrame]);
        if (!VkResultIsSuccess(resetRes))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "vkResetFences failed: %s", VkResultMessage(resetRes, true).c_str());
            return FrameResult::ERROR;
        }
    }

    // Collect command buffers for this frame
    std::array<VkCommandBuffer, 3> cmdBuffers =
            {
                    vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex].handle,
                    vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex].handle,
                    vkContext->graphicsCommandBuffers[vkContext->imageIndex].handle
            };

    // Submit to graphics queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = static_cast<uint32>(cmdBuffers.size());
    submitInfo.pCommandBuffers    = cmdBuffers.data();

    // Signal when graphics queue is done
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &vkContext->queueCompleteSemaphores[vkContext->currentFrame];

    // Wait on the "image available" semaphore before executing CBs
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = &vkContext->imageAvailableSemaphores[vkContext->currentFrame];

    // IMPORTANT: wait stage mask count MUST equal waitSemaphoreCount (was mismatched before)
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.pWaitDstStageMask = waitStages;

    VkResult submitRes;
    {
        std::lock_guard<std::mutex> queueLock(vkContext->device.graphicsQueueMutex);
        submitRes = vkQueueSubmit(
                vkContext->device.graphicsQueue,
                1, &submitInfo,
                vkContext->inFlightFences[vkContext->currentFrame]);
    }

    if (!VkResultIsSuccess(submitRes))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "vkQueueSubmit failed: %s", VkResultMessage(submitRes, true).c_str());
        return FrameResult::ERROR;
    }

    // Mark CBs as submitted (your helper)
    {
        NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(
                &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex]);
        NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(
                &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex]);
        NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(
                &vkContext->graphicsCommandBuffers[vkContext->imageIndex]);
    }

    // Present
    VkResult presentRes = NOUS_VulkanSwapChain::SwapChainPresent(
            vkContext,
            &vkContext->swapChain,
            vkContext->device.graphicsQueue,
            vkContext->device.presentQueue,
            vkContext->queueCompleteSemaphores[vkContext->currentFrame],
            vkContext->imageIndex);

    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
    {
        // Trigger recreation path and skip this frame; not fatal.
        vkContext->recreatingSwapchain = true;
        NOUS_INFO_C(CURRENT_CHANNEL, "Queue present returned %s. Scheduling recreation; skipping frame.",
                  VkResultMessage(presentRes, true).c_str());
        return FrameResult::SKIPPED;
    }

    if (!VkResultIsSuccess(presentRes))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Queue present failed: %s", VkResultMessage(presentRes, true).c_str());
        return FrameResult::ERROR;
    }

    return FrameResult::SUCCESS;
}

bool VulkanBackend::BeginRenderpass(RenderpassType renderpassID)
{
    // Begin recording commands.
    VulkanCommandBuffer* commandBuffer = nullptr;
    VulkanRenderpass* renderpass = nullptr;
    VkFramebuffer framebuffer = 0;
    
    switch (renderpassID)
    {
        case RenderpassType::SCENE:
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->sceneRenderpass;
            framebuffer = vkContext->imGuiResources.m_ViewportFramebuffers[vkContext->imageIndex];
            break;
        }
        case RenderpassType::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->gameRenderpass;
            framebuffer = vkContext->imGuiResources.m_GameViewportFramebuffers[vkContext->imageIndex];
            break;
        }
        case RenderpassType::UI:
        {
            commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->uiRenderpass;
            framebuffer = vkContext->swapChain.swapChainFramebuffers[vkContext->imageIndex];
            break;
        }
        default:
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Vulkan Renderpass called on an unrecognized renderpass ID.");
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

    renderpass->renderArea.z = vkContext->framebufferWidth;
    renderpass->renderArea.w = vkContext->framebufferHeight;

    NOUS_VulkanRenderpass::BeginRenderpass(commandBuffer, renderpass, framebuffer);

    // Initial pipeline bind; UpdateGlobalWorldState / DrawGeometry will re-bind as needed.
    auto TryBind = [&](ResourceShader* rs) {
        if (rs && rs->internalData)
            NOUS_VulkanShader::BindPipeline(commandBuffer->handle,
                static_cast<VulkanShader*>(rs->internalData));
    };

    switch (renderpassID)
    {
        case RenderpassType::SCENE: TryBind(vkContext->builtInMaterialShader); break;
        case RenderpassType::GAME:  TryBind(vkContext->builtInGameShader);     break;
        case RenderpassType::UI:    break; // ImGui uses its own imgui_impl_vulkan pipeline
    }

    return true;
}

bool VulkanBackend::EndRenderpass(RenderpassType renderpassID)
{
    VulkanRenderpass* renderpass = nullptr;
    VulkanCommandBuffer* commandBuffer = nullptr;
    
    switch (renderpassID)
    {
        case RenderpassType::SCENE:
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->sceneRenderpass;
            break;
        }
        case RenderpassType::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->gameRenderpass;
            break;
        }
        case RenderpassType::UI:
        {
            commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->uiRenderpass;
            break;
        }
        default:
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Vulkan Renderpass called on an unrecognized renderpass ID.");
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
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Recreate Swapchain called when already recreating. Booting.");
        return false;
    }

    // Detect if the window is too small to be drawn to.
    if (vkContext->framebufferWidth == 0 || vkContext->framebufferHeight == 0)
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Recreate Swapchain called when window is < 1 in a dimension. Booting.");
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
    vkContext->device.swapChainSupport = NOUS_VulkanDevice::QuerySwapChainSupport(vkContext->device.physicalDevice, vkContext);
    vkContext->device.depthFormat = NOUS_VulkanDevice::FindDepthFormat(vkContext->device.physicalDevice);

    NOUS_VulkanSwapChain::RecreateSwapChain(vkContext, cachedFramebufferWidth, cachedFramebufferHeight, &vkContext->swapChain);

    // Sync the framebuffer size with the cached sizes.
    vkContext->framebufferWidth = cachedFramebufferWidth;
    vkContext->framebufferHeight = cachedFramebufferHeight;

    External->BroadcastEvent(Event(EventType::IMGUI_RECREATION, {}));

    cachedFramebufferWidth = 0;
    cachedFramebufferHeight = 0;

    // Update framebuffer size generation.
    vkContext->framebufferSizeLastGeneration = vkContext->framebufferSizeGeneration;

    // Free old command buffers and framebuffers before recreating them.
    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &vkContext->graphicsCommandBuffers[i]);
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &vkContext->imGuiResources.m_ViewportCommandBuffers[i]);
        NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool, &vkContext->imGuiResources.m_GameViewportCommandBuffers[i]);
    }

    if (vkContext->imGuiResources.m_PickFramebuffer)
    {
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_PickFramebuffer, vkContext->allocator);
        vkContext->imGuiResources.m_PickFramebuffer = VK_NULL_HANDLE;
    }

    for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
    {
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_ViewportFramebuffers[i], vkContext->allocator);
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->imGuiResources.m_GameViewportFramebuffers[i], vkContext->allocator);
        vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->swapChain.swapChainFramebuffers[i], vkContext->allocator);
    }

    // Update renderpass areas to match the new framebuffer dimensions.
    auto updateRenderArea = [&](VulkanRenderpass& rp) {
        rp.renderArea = { 0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight };
    };
    updateRenderArea(vkContext->sceneRenderpass);
    updateRenderArea(vkContext->pickRenderpass);
    updateRenderArea(vkContext->gameRenderpass);
    updateRenderArea(vkContext->uiRenderpass);

    // Regenerate world framebuffers
    NOUS_VulkanFramebuffer::CreateFramebuffers(vkContext);

    NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext);

    // Clear the recreating flag.
    vkContext->recreatingSwapchain = false;

    return true;
}

bool VulkanBackend::UpdateGlobalWorldState(
        RenderpassType renderpassID,
        const glm::mat4& projection, const glm::mat4& view,
        const glm::vec3& viewPosition, const glm::vec4& ambientColor,
        int32 mode)
{
    ResourceShader* rShader = (renderpassID == RenderpassType::GAME)
        ? vkContext->builtInGameShader
        : vkContext->builtInMaterialShader;

    if (!rShader || !rShader->internalData) return false;

    VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    VulkanShader* vs = static_cast<VulkanShader*>(rShader->internalData);

    NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vs);

    struct GlobalUBO { glm::mat4 projection; glm::mat4 view; } ubo{ projection, view };
    NOUS_VulkanShader::UpdateGlobal(vkContext, commandBuffer->handle, vs,
        vkContext->imageIndex, &ubo, sizeof(ubo));

    return true;
}


VulkanCommandBuffer* VulkanBackend::GetCommandBufferByRenderpassID(RenderpassType renderpassID)
{
    VulkanCommandBuffer* commandBuffer = nullptr;

    switch (renderpassID)
    {
        case RenderpassType::SCENE:
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            break;
        }
        case RenderpassType::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            break;
        }
        case RenderpassType::UI:
        {
            commandBuffer = &vkContext->graphicsCommandBuffers[vkContext->imageIndex];
            break;
        }
        default:
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Vulkan Renderpass called on an unrecognized renderpass ID.");
        }
    }

    return commandBuffer;
}

bool VulkanBackend::DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData)
{
    if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID)
        return true;

    ResourceShader* rShader = (renderpassID == RenderpassType::GAME)
        ? vkContext->builtInGameShader
        : vkContext->builtInMaterialShader;

    if (!rShader || !rShader->internalData) return false;

    VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    VulkanShader*        vs            = static_cast<VulkanShader*>(rShader->internalData);
    VulkanGeometryData*  bufferData    = &vkContext->geometries[renderData.geometry->internalID];

    // Bind pipeline.
    NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vs);

    // Push model matrix via push constants.
    vkCmdPushConstants(commandBuffer->handle, vs->pipeline.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &renderData.model);

    // Resolve material.
    ResourceMaterial* material = renderData.material
        ? renderData.material
        : External->resourceManager->GetDefaultMaterial();

    // Per-instance descriptors (material UBO + texture sampler).
    if (material && material->internalID != INVALID_ID && vs->instancePool)
    {
        const uint32_t instanceID  = material->internalID;
        const uint32_t imageIndex  = vkContext->imageIndex;

        // Write diffuse colour to instance UBO (binding 0).
        struct InstanceUBO { glm::vec4 diffuseColor; } ubo{ material->diffuseColor };
        auto& uboGen = vs->instanceStates[instanceID].descriptorStates[0].generations[imageIndex];
        NOUS_VulkanShader::WriteInstanceUBO(vkContext, vs, imageIndex, instanceID,
            &ubo, sizeof(ubo), &uboGen);

        // Write diffuse texture sampler (binding 1), lazy update.
        // Fall back to the default texture if the material's texture is missing,
        // has an invalid generation, or has not yet been uploaded to the GPU.
        ResourceTexture* texture = material->diffuseMap.texture;
        if (!texture || texture->generation == INVALID_ID || !texture->internalData)
            texture = External->resourceManager->GetDefaultTexture();

        if (texture && texture->internalData)
        {
            VulkanTextureData* texData = static_cast<VulkanTextureData*>(texture->internalData);
            auto& samplerGen = vs->instanceStates[instanceID].descriptorStates[1].generations[imageIndex];
            auto& samplerID  = vs->instanceStates[instanceID].descriptorStates[1].ids[imageIndex];
            NOUS_VulkanShader::WriteInstanceSampler(vkContext, vs, imageIndex, instanceID,
                1, texData->image.view, texData->sampler,
                &samplerGen, &samplerID, texture->ID, texture->generation);
        }
        else if (vs->instanceStates[instanceID].descriptorStates[1].generations[imageIndex] == UINT32_MAX)
        {
            // Binding 1 has never been written for this image index (fresh slot, no
            // valid texture yet including no default). Drawing now would violate
            // VUID-vkCmdDrawIndexed-None-08114 — skip until a texture is available.
            return true;
        }

        NOUS_VulkanShader::BindInstanceDescriptorSet(commandBuffer->handle, vs, imageIndex, instanceID);
    }

    // Bind vertex buffer.
    VkDeviceSize offset = bufferData->vertexBufferOffset;
    vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1,
        &vkContext->objectVertexBuffer.handle, &offset);

    if (bufferData->indexCount > 0)
    {
        vkCmdBindIndexBuffer(commandBuffer->handle, vkContext->objectIndexBuffer.handle,
            bufferData->indexBufferOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer->handle, bufferData->indexCount, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(commandBuffer->handle, bufferData->vertexCount, 1, 0, 0);
    }

    return true;
}

// ----------------------------------------------------------------------------------------------- //
// TEMPORAL //

bool VulkanBackend::CreateTexture(const uint8* pixels, ResourceTexture* texture)
{
    // Internal data creation.
    // TODO: Use an allocator for this.
    texture->internalData = reinterpret_cast<VulkanTextureData*>(
            NOUS_NEW<VulkanTextureData>(MemoryTag::RESOURCE_TEXTURE));

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
    NOUS_VulkanImage::CreateVulkanImage(vkContext, VK_IMAGE_TYPE_2D, texture->width, texture->height, 1, VK_SAMPLE_COUNT_1_BIT, imageFormat,
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
    NOUS_VulkanImage::TransitionVulkanImageLayout(vkContext, &tempCommandBuffer, &textureData->image, imageFormat,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the data from the buffer.
    NOUS_VulkanImage::CopyBufferToVulkanImage(vkContext, &textureData->image, stagingBuffer.handle, &tempCommandBuffer);

    // Transition from optimal for data reciept to shader-read-only optimal layout.
    NOUS_VulkanImage::TransitionVulkanImageLayout(vkContext, &tempCommandBuffer, &textureData->image, imageFormat,
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
        NOUS_ERROR_C(CURRENT_CHANNEL, "[%s][VULKAN] Error creating texture sampler: %s", VkResultMessage(result, true).c_str());
        return false;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "[%s][VULKAN] Texture Created Successfully: %s",
              __FUNCTION__, VkResultMessage(result, true).c_str());
    return true;
}

void VulkanBackend::DestroyTexture(ResourceTexture* texture) noexcept
{
    VulkanTextureData* textureData = reinterpret_cast<VulkanTextureData*>(texture->internalData);

    if (textureData)
    {
        // Ensure no in-flight command buffers still reference this image/view before freeing.
        vkDeviceWaitIdle(vkContext->device.logicalDevice);

        NOUS_VulkanImage::DestroyVulkanImage(vkContext, &textureData->image);
        MemoryManager::ZeroMemory(&textureData->image, sizeof(VulkanImage));

        vkDestroySampler(vkContext->device.logicalDevice, textureData->sampler, vkContext->allocator);
        textureData->sampler = 0;

        NOUS_DELETE(textureData, MemoryTag::RESOURCE_TEXTURE);
    }
}

bool VulkanBackend::CreateMaterial(ResourceMaterial* material)
{
    if (!material)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::CreateMaterial() called with nullptr.");
        return false;
    }

    // Guard: slot already acquired (shared resource requested a second time).
    if (material->internalID != INVALID_ID)
        return true;

    // Acquire an instance slot from the scene shader.
    // The game shader uses the same GLSL/layout so slots are acquired in sync.
    if (vkContext->builtInMaterialShader && vkContext->builtInMaterialShader->internalData)
    {
        VulkanShader* vs = static_cast<VulkanShader*>(vkContext->builtInMaterialShader->internalData);
        uint32_t instanceID = 0;
        if (!NOUS_VulkanShader::AcquireInstanceSlot(vkContext, vs, &instanceID))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::CreateMaterial() - Instance pool full.");
            return false;
        }
        material->internalID = instanceID;

        // Acquire the matching slot in the game shader as well.
        if (vkContext->builtInGameShader && vkContext->builtInGameShader->internalData)
        {
            VulkanShader* vsGame = static_cast<VulkanShader*>(vkContext->builtInGameShader->internalData);
            uint32_t gameID = 0;
            NOUS_VulkanShader::AcquireInstanceSlot(vkContext, vsGame, &gameID);
            // gameID should match instanceID since both pools start empty and are
            // acquired in the same order.
        }
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Material created (instance %u).", __FUNCTION__, material->internalID);
    return true;
}

void VulkanBackend::DestroyMaterial(ResourceMaterial* material) noexcept
{
    if (material)
    {
        if (material->internalID != INVALID_ID)
        {
            // Ensure descriptor sets are no longer referenced by any in-flight command buffers.
            vkDeviceWaitIdle(vkContext->device.logicalDevice);

            if (vkContext->builtInMaterialShader && vkContext->builtInMaterialShader->internalData)
            {
                VulkanShader* vs = static_cast<VulkanShader*>(vkContext->builtInMaterialShader->internalData);
                NOUS_VulkanShader::ReleaseInstanceSlot(vkContext, vs, material->internalID);
            }
            if (vkContext->builtInGameShader && vkContext->builtInGameShader->internalData)
            {
                VulkanShader* vsGame = static_cast<VulkanShader*>(vkContext->builtInGameShader->internalData);
                NOUS_VulkanShader::ReleaseInstanceSlot(vkContext, vsGame, material->internalID);
            }
            material->internalID = INVALID_ID;
        }
        else
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "VulkanBackend::DestroyMaterial() called with INVALID_ID.");
        }
    }
    else 
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "VulkanBackend::DestroyMaterial() called with nullptr. Nothing was done.");
    }
}

bool VulkanBackend::CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* geometry)
{
    if (!vertexCount || !vertices) 
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::CreateGeometry() requires vertex data, and none was supplied. vertexCount=%d, vertices=%p", vertexCount, vertices);
        return false;
    }

    // Check if this is a re-upload. If it is, need to free old data afterward.
    // TODO: ResourceManager needs to take care of reuploads, not here.
    
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
        std::lock_guard<std::mutex> geoLock(vkContext->geometriesMutex);
        for (uint32 i = 0; i < VULKAN_MAX_GEOMETRY_COUNT; ++i)
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
        NOUS_FATAL_C(CURRENT_CHANNEL, "VulkanBackend::CreateGeometry() failed to find a free index for a new geometry upload. Adjust config to allow for more.");
        return false;
    }

    VkCommandPool pool = NOUS_VulkanMultithreading::GetThreadCommandPool(vkContext, NOUS_Multithreading::NOUS_Thread::GetThreadID(std::this_thread::get_id()));
    VkQueue queue = vkContext->device.graphicsQueue;

    // Vertex data.
    internalData->vertexCount = vertexCount;
    internalData->vertexSize = sizeof(Vertex3D) * vertexCount;

    {
        std::lock_guard<std::mutex> vtxLock(vkContext->vertexBufferMutex);
        if (!NOUS_VulkanBuffer::UploadDataRange(vkContext, pool, 0, queue, &vkContext->objectVertexBuffer,
            &internalData->vertexBufferOffset, internalData->vertexSize, vertices))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::CreateGeometry() failed to upload to the vertex buffer!");
            return false;
        }
    }

    // Index data, if applicable
    if (indexCount && indices)
    {
        internalData->indexCount = indexCount;
        internalData->indexSize = sizeof(uint32) * indexCount;

        std::lock_guard<std::mutex> idxLock(vkContext->indexBufferMutex);
        if (!NOUS_VulkanBuffer::UploadDataRange(vkContext, pool, 0, queue, &vkContext->objectIndexBuffer,
            &internalData->indexBufferOffset, internalData->indexSize, indices))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::CreateGeometry() failed to upload to the index buffer!");
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
        {
            std::lock_guard<std::mutex> vtxLock(vkContext->vertexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectVertexBuffer,
                oldRange.vertexBufferOffset, oldRange.vertexSize);
        }

        // Free index data, if applicable
        if (oldRange.indexSize > 0)
        {
            std::lock_guard<std::mutex> idxLock(vkContext->indexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectIndexBuffer,
                oldRange.indexBufferOffset, oldRange.indexSize);
        }
    }

    return true;
}

void VulkanBackend::DestroyGeometry(ResourceMesh* geometry) noexcept
{
    if (geometry && geometry->internalID != INVALID_ID)
    {
        // Ensure no in-flight draw commands are still reading from these buffer ranges.
        vkDeviceWaitIdle(vkContext->device.logicalDevice);

        VulkanGeometryData* internalData = &vkContext->geometries[geometry->internalID];

        // Free vertex data
        {
            std::lock_guard<std::mutex> vtxLock(vkContext->vertexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectVertexBuffer, internalData->vertexBufferOffset, internalData->vertexSize);
        }

        // Free index data, if applicable
        if (internalData->indexSize > 0)
        {
            std::lock_guard<std::mutex> idxLock(vkContext->indexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectIndexBuffer, internalData->indexBufferOffset, internalData->indexSize);
        }

        // Clean up data.
        {
            std::lock_guard<std::mutex> geoLock(vkContext->geometriesMutex);
            MemoryManager::ZeroMemory(internalData, sizeof(VulkanGeometryData));
            internalData->ID = INVALID_ID;
            internalData->generation = INVALID_ID;
        }
    }
}

// ─────────────────────────────── Shaders ─────────────────────────────────

bool VulkanBackend::CreateShader(ResourceShader* shader)
{
    if (!shader)
        return false;

    const std::string assetPath = shader->GetAssetsPath();

    // ── BuiltIn.MaterialShader → scene renderpass (primary) ───────────────────
    //    Also creates an internal clone for the game renderpass so both viewports
    //    have independent global UBO buffers and descriptor sets.
    if (assetPath.find("BuiltIn.MaterialShader") != std::string::npos)
    {
        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader))
            return false;
        vkContext->builtInMaterialShader = shader;
        NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.MaterialShader assigned to sceneRenderpass.");

        // Game renderpass clone — owns its own VulkanShader (not in ResourceManager).
        ResourceShader* gameShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
        gameShader->stagesData = shader->stagesData;
        gameShader->reflection = shader->reflection;

        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->gameRenderpass, gameShader))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[CreateShader] Failed to create game-renderpass variant; game viewport will be unavailable.");
            NOUS_DELETE(gameShader, MemoryTag::RESOURCE_SHADER);
        }
        else
        {
            vkContext->builtInGameShader = gameShader;
            NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.MaterialShader clone assigned to gameRenderpass.");
        }

        return true;
    }

    // ── BuiltIn.PickShader → scene renderpass (for mouse picking) ──────────────
    //    Internal clone — not tracked by ResourceManager. Stored as builtInPickShader.
    if (assetPath.find("BuiltIn.PickShader") != std::string::npos)
    {
        ResourceShader* pickShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
        pickShader->stagesData = shader->stagesData;
        pickShader->reflection = shader->reflection;

        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->pickRenderpass, pickShader, true))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[CreateShader] Failed to create BuiltIn.PickShader.");
            NOUS_DELETE(pickShader, MemoryTag::RESOURCE_SHADER);
            return false;
        }

        vkContext->builtInPickShader = pickShader;
        NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.PickShader assigned to pickRenderpass.");
        return true;
    }

    // ── BuiltIn.OutlineShader → scene renderpass only (editor view, not game) ──
    //    The outline effect is an editor-only feature; no game renderpass clone needed.
    if (assetPath.find("BuiltIn.OutlineShader") != std::string::npos)
    {
        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader,
                                        /*disableBlending=*/false, /*createOutlinePipelines=*/true))
            return false;
        vkContext->builtInOutlineShader = shader;
        NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.OutlineShader assigned to sceneRenderpass.");
        return true;
    }

    // ── Default: scene renderpass for user-defined shaders ────────────────────
    return NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader);
}

void VulkanBackend::DestroyShader(ResourceShader* shader) noexcept
{
    if (!shader || !shader->internalData)
        return;

    // Null out vkContext built-in pointer so Shutdown() doesn't touch freed memory.
    if (shader == vkContext->builtInMaterialShader) vkContext->builtInMaterialShader = nullptr;
    if (shader == vkContext->builtInOutlineShader)  vkContext->builtInOutlineShader  = nullptr;

    VulkanShader* vs = static_cast<VulkanShader*>(shader->internalData);
    NOUS_VulkanShader::Destroy(vkContext, vs);
    shader->internalData = nullptr;
}

uint32 VulkanBackend::PickObjectAt(int32 pixelX, int32 pixelY,
                                   const glm::mat4& projection, const glm::mat4& view,
                                   const std::vector<GeometryRenderData>& geometries)
{
    ResourceShader* rPickShader = vkContext->builtInPickShader;
    if (!rPickShader || !rPickShader->internalData)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[PickObjectAt] Pick shader not available.");
        return 0;
    }

    // Clamp pixel coordinates to framebuffer bounds.
    if (pixelX < 0 || pixelX >= vkContext->framebufferWidth ||
        pixelY < 0 || pixelY >= vkContext->framebufferHeight)
    {
        return 0;
    }

    VulkanShader* pickVS = static_cast<VulkanShader*>(rPickShader->internalData);

    // Wait for all GPU work to complete before using the pick resources.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // --- Allocate single-use command buffer ---
    VulkanCommandBuffer cmdBuffer;
    VkCommandPool pool = vkContext->device.mainGraphicsCommandPool;
    VkQueue queue = vkContext->device.graphicsQueue;
    NOUS_VulkanCommandBuffer::CommandBufferAllocateAndBeginSingleTime(vkContext, pool, &cmdBuffer);

    // --- Set viewport and scissor ---
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(vkContext->framebufferHeight);
    viewport.width = static_cast<float>(vkContext->framebufferWidth);
    viewport.height = -static_cast<float>(vkContext->framebufferHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = { static_cast<uint32>(vkContext->framebufferWidth),
                       static_cast<uint32>(vkContext->framebufferHeight) };
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &scissor);

    // --- Begin renderpass (dedicated UNORM pick renderpass) ---
    VulkanRenderpass pickRP = vkContext->pickRenderpass;
    pickRP.renderArea = { 0, 0, static_cast<float>(vkContext->framebufferWidth),
                                static_cast<float>(vkContext->framebufferHeight) };
    pickRP.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f }; // objectUID 0 = "no object"
    NOUS_VulkanRenderpass::BeginRenderpass(&cmdBuffer, &pickRP, vkContext->imGuiResources.m_PickFramebuffer);

    // --- Bind pick pipeline ---
    NOUS_VulkanShader::BindPipeline(cmdBuffer.handle, pickVS);

    // --- Update global UBO (projection + view) ---
    struct GlobalUBO { glm::mat4 projection; glm::mat4 view; } ubo{ projection, view };
    NOUS_VulkanShader::UpdateGlobal(vkContext, cmdBuffer.handle, pickVS, 0, &ubo, sizeof(ubo));

    // --- Draw each geometry with objectUID push constant ---
    struct PickPushConstants
    {
        glm::mat4 model;
        uint32 objectID;
    };

    for (const auto& geo : geometries)
    {
        if (!geo.geometry || geo.geometry->internalID == INVALID_ID)
            continue;

        VulkanGeometryData* bufferData = &vkContext->geometries[geo.geometry->internalID];

        PickPushConstants pc{};
        pc.model = geo.model;
        pc.objectID = geo.objectUID;

        vkCmdPushConstants(cmdBuffer.handle, pickVS->pipeline.pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(PickPushConstants), &pc);

        // Bind vertex buffer
        VkDeviceSize offset = bufferData->vertexBufferOffset;
        vkCmdBindVertexBuffers(cmdBuffer.handle, 0, 1,
            &vkContext->objectVertexBuffer.handle, &offset);

        if (bufferData->indexCount > 0)
        {
            vkCmdBindIndexBuffer(cmdBuffer.handle, vkContext->objectIndexBuffer.handle,
                bufferData->indexBufferOffset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmdBuffer.handle, bufferData->indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(cmdBuffer.handle, bufferData->vertexCount, 1, 0, 0);
        }
    }

    // --- End renderpass ---
    NOUS_VulkanRenderpass::EndRenderpass(&cmdBuffer, &pickRP);

    // --- Transition pick image for transfer readback ---
    // Scene renderpass transitions to SHADER_READ_ONLY; we need TRANSFER_SRC for copy.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkContext->imGuiResources.m_PickImage.handle;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuffer.handle,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // --- Create staging buffer for single-pixel readback ---
    VulkanBuffer stagingBuffer{};
    NOUS_VulkanBuffer::CreateBuffer(vkContext, 4,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true, &stagingBuffer);

    // --- Copy single pixel from pick image to staging buffer ---
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { pixelX, pixelY, 0 };
    region.imageExtent = { 1, 1, 1 };

    vkCmdCopyImageToBuffer(cmdBuffer.handle,
        vkContext->imGuiResources.m_PickImage.handle,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer.handle, 1, &region);

    // --- Submit and wait ---
    NOUS_VulkanCommandBuffer::CommandBufferEndAndFreeSingleTime(vkContext, pool, &cmdBuffer, queue);

    // --- Read back pixel data ---
    uint32 objectID = 0;
    void* mapped = NOUS_VulkanBuffer::LockMemory(vkContext, &stagingBuffer, 0, 4, 0);
    if (mapped)
    {
        const uint8* pixel = static_cast<const uint8*>(mapped);
        objectID = static_cast<uint32>(pixel[0])
                 | (static_cast<uint32>(pixel[1]) << 8)
                 | (static_cast<uint32>(pixel[2]) << 16)
                 | (static_cast<uint32>(pixel[3]) << 24);
        NOUS_VulkanBuffer::UnlockMemory(vkContext, &stagingBuffer);
    }

    // --- Cleanup staging buffer ---
    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &stagingBuffer);

    NOUS_DEBUG_C(CURRENT_CHANNEL, "[PickObjectAt] Pixel (%d, %d) -> objectUID = %u", pixelX, pixelY, objectID);

    return objectID;
}

VulkanContext* VulkanBackend::GetVulkanContext()
{
    return vkContext;
}

bool VulkanBackend::DrawOutlinedGeometries(RenderpassType renderpassID,
                                            const glm::mat4& projection, const glm::mat4& view,
                                            const std::vector<GeometryRenderData>& outlinedGeometries,
                                            const OutlineSettings& settings)
{
    // Outline effect is scene-viewport only.
    if (renderpassID != RenderpassType::SCENE)
        return true;

    if (outlinedGeometries.empty())
        return true;

    ResourceShader* rOutlineShader = vkContext->builtInOutlineShader;
    if (!rOutlineShader || !rOutlineShader->internalData)
        return true; // Outline shader not loaded yet — skip gracefully.

    VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    VulkanShader* vs = static_cast<VulkanShader*>(rOutlineShader->internalData);

    // Upload the outline global UBO: projection + view + outlineColor.
    struct OutlineGlobalUBO { glm::mat4 projection; glm::mat4 view; glm::vec4 outlineColor; }
        globalUBO{ projection, view, settings.color };

    // ── Pass 1: Stencil-write ─────────────────────────────────────────────────
    // Draw the selected mesh at its original scale. The stencil-write pipeline writes
    // stencil=1 for every visible pixel (depth test ON, colour write OFF).

    NOUS_VulkanShader::BindStencilWritePipeline(commandBuffer->handle, vs);
    NOUS_VulkanShader::UpdateGlobal(vkContext, commandBuffer->handle, vs,
        vkContext->imageIndex, &globalUBO, sizeof(globalUBO));

    for (const auto& renderData : outlinedGeometries)
    {
        if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID) continue;

        VulkanGeometryData* bufferData = &vkContext->geometries[renderData.geometry->internalID];

        vkCmdPushConstants(commandBuffer->handle, vs->pipeline.pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &renderData.model);

        VkDeviceSize offset = bufferData->vertexBufferOffset;
        vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1,
            &vkContext->objectVertexBuffer.handle, &offset);

        if (bufferData->indexCount > 0)
        {
            vkCmdBindIndexBuffer(commandBuffer->handle, vkContext->objectIndexBuffer.handle,
                bufferData->indexBufferOffset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer->handle, bufferData->indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer->handle, bufferData->vertexCount, 1, 0, 0);
        }
    }

    // ── Pass 2: Outline-draw ──────────────────────────────────────────────────
    // Draw each mesh again with a uniformly scaled model matrix. The outline-draw
    // pipeline uses stencil NOTEQUAL(1), so only the border ring is coloured.

    NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vs);

    for (const auto& renderData : outlinedGeometries)
    {
        if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID) continue;

        VulkanGeometryData* bufferData = &vkContext->geometries[renderData.geometry->internalID];

        // Scale uniformly around the model's own origin.
        const glm::mat4 scaledModel = glm::scale(renderData.model, glm::vec3(settings.width));

        vkCmdPushConstants(commandBuffer->handle, vs->pipeline.pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &scaledModel);

        VkDeviceSize offset = bufferData->vertexBufferOffset;
        vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1,
            &vkContext->objectVertexBuffer.handle, &offset);

        if (bufferData->indexCount > 0)
        {
            vkCmdBindIndexBuffer(commandBuffer->handle, vkContext->objectIndexBuffer.handle,
                bufferData->indexBufferOffset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer->handle, bufferData->indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer->handle, bufferData->vertexCount, 1, 0, 0);
        }
    }

    return true;
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
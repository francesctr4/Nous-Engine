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
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include <FileSystem/FileSystem.h>

#include <MemoryManager/MemoryManager.h>
#include <Logger/Logger.h>

#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Types/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"
#include <ShaderSystem/ShaderLoader/ShaderLoader.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompilerTypes.h>
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif
#include "Engine/Renderer/Backend/Vulkan/Resources/Shader/VulkanShader.h"

#include <NOUS_Multithreading/NOUS_Thread.h>
#include "Engine/Utils/Math/Vertex.inl"
#include <Engine/Core/EventSystem/EventSystem.h>
#include "Engine/Renderer/iRenderWindow.h"
#include "Engine/Renderer/iRenderResourceProvider.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_BACKEND_VULKAN_BACKEND;

VulkanBackend::VulkanBackend()
{
    vkContext = NOUS_NEW<VulkanContext>(MemoryTag::RENDERER);
}

VulkanBackend::~VulkanBackend()
{
    NOUS_DELETE(vkContext, MemoryTag::RENDERER);
}

void VulkanBackend::InjectDependencies(
    EventSystem* eventSystem,
    nous::engine::multithreading::NOUS_JobSystem* jobSystem,
    IRenderWindow* window,
    IRenderResourceProvider* resourceProvider)
{
    vkContext->eventSystem     = eventSystem;
    vkContext->jobSystem       = jobSystem;
    vkContext->window          = window;
    vkContext->resourceManager = resourceProvider;
}

bool VulkanBackend::Initialize()
{
    bool ret = true;

    NOUS_INFO_C(CURRENT_CHANNEL, " ----------------------- USING VULKAN BACKEND ----------------------- ");

    // TODO: Custom allocator
    vkContext->allocator = nullptr;

    // Get Framebuffer Size
    int32 initialWidth = 0, initialHeight = 0;
    vkContext->window->GetFramebufferSize(&initialWidth, &initialHeight);

    vkContext->framebufferWidth  = (initialWidth  != 0) ? initialWidth  : WINDOW_WIDTH;
    vkContext->framebufferHeight = (initialHeight != 0) ? initialHeight : WINDOW_HEIGHT;

    // Instance
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan instance...");
    if (!NOUS_VulkanInstance::CreateInstance(vkContext))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Instance. Shutting the Application.");
        return false;
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
        return false;
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
        return false;
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
        return false;
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
        return false;
    }
    else
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Swap Chain created successfully!");
    }

    NOUS_ImGuiVulkanResources::CreateImGuiVulkanResources(vkContext);

    if (vkContext->renderMode == RenderMode::GAME)
    {
        // GAME mode: single non-offscreen renderpass writing directly to swapchain.
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Creating Vulkan Game Swapchain Render Pass (GAME mode)...");
        if (!NOUS_VulkanRenderpass::CreateRenderpass(vkContext, &vkContext->gameSwapchainRenderpass,
            glm::vec4(0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight),
            glm::vec4(0.0f, 0.0f, 0.1f, 1.0f),
            1.0f, 0,
            RenderpassClearFlag::COLOR_BUFFER | RenderpassClearFlag::DEPTH_BUFFER | RenderpassClearFlag::STENCIL_BUFFER,
            false, false, false))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create Vulkan Game Swapchain Render Pass. Shutting the Application.");
            ret = false;
        }
        else
        {
            NOUS_DEBUG_C(CURRENT_CHANNEL, "Vulkan Game Swapchain Render Pass created successfully!");
        }
    }
    else
    {
        // EDITOR mode: all renderpasses.

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

        // Game Render Pass (offscreen viewport)
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

    // ── Instance SSBO (per-frame, triple-buffered, persistently mapped) ──────────
    // Buffer holds 2×c_maxInstances matrices: scene pass uses indices [0, c_maxInstances),
    // game pass uses indices [c_maxInstances, 2*c_maxInstances). This prevents the game-pass
    // upload from overwriting scene matrices before the GPU reads them.
    {
        const VkDeviceSize ssboSize = 2 * c_maxInstances * sizeof(glm::mat4);
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, ssboSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    true, &vkContext->instanceSSBO[i]))
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "[Initialize] Failed to create instance SSBO %u.", i);
                return false;
            }
            vkMapMemory(vkContext->device.logicalDevice,
                        vkContext->instanceSSBO[i].memory, 0, ssboSize, 0,
                        &vkContext->instanceSSBOMapped[i]);
        }
        NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Instance SSBOs created (3 × %llu bytes, scene+game split).", (unsigned long long)ssboSize);
    }

    if (vkContext->renderMode == RenderMode::GAME)
    {
        // GAME mode: no editor-only vertex buffers needed.
        return ret;
    }

    // ── Create editor grid vertex buffer ──────────────────────────────────────
    {
        constexpr int halfExtent = 500;
        constexpr int step       = 10;

        constexpr glm::vec3 colorAxisX(0.70f, 0.15f, 0.15f); // X axis: red
        constexpr glm::vec3 colorAxisZ(0.15f, 0.15f, 0.70f); // Z axis: blue
        constexpr glm::vec3 colorMinor(0.30f, 0.30f, 0.30f); // regular lines: grey

        constexpr auto fHalf = static_cast<float>(halfExtent);

        // Layout: axis lines first (4 vertices), then all minor lines.
        // This lets DrawGrid issue two draw calls with different line widths.
        std::vector<Vertex3D> gridVerts;
        gridVerts.reserve((halfExtent * 2 / step + 1) * 4);

        // ── Axis lines (always first, 4 vertices) ─────────────────────────────
        { Vertex3D v{}; v.position = {-fHalf, 0.0f, 0.0f}; v.color = colorAxisX; gridVerts.push_back(v); }
        { Vertex3D v{}; v.position = { fHalf, 0.0f, 0.0f}; v.color = colorAxisX; gridVerts.push_back(v); }
        { Vertex3D v{}; v.position = {0.0f, 0.0f, -fHalf}; v.color = colorAxisZ; gridVerts.push_back(v); }
        { Vertex3D v{}; v.position = {0.0f, 0.0f,  fHalf}; v.color = colorAxisZ; gridVerts.push_back(v); }

        // ── Minor lines (skip i==0, those are the axes) ───────────────────────
        for (int i = -halfExtent; i <= halfExtent; i += step)
        {
            if (i == 0) continue;

            const auto fi = static_cast<float>(i);

            Vertex3D v1{}, v2{};
            v1.position = {-fHalf, 0.0f, fi};
            v2.position = { fHalf, 0.0f, fi};
            v1.color = v2.color = colorMinor;
            gridVerts.push_back(v1);
            gridVerts.push_back(v2);

            Vertex3D v3{}, v4{};
            v3.position = {fi, 0.0f, -fHalf};
            v4.position = {fi, 0.0f,  fHalf};
            v3.color = v4.color = colorMinor;
            gridVerts.push_back(v3);
            gridVerts.push_back(v4);
        }

        vkContext->gridVertexCount = static_cast<uint32>(gridVerts.size());
        const uint64 bufferSize    = gridVerts.size() * sizeof(Vertex3D);

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, bufferSize,
            static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, &vkContext->gridVertexBuffer))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[Initialize] Failed to create grid vertex buffer.");
        }
        else
        {
            NOUS_VulkanBuffer::LoadData(vkContext, &vkContext->gridVertexBuffer,
                0, bufferSize, 0, gridVerts.data());
            NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Grid vertex buffer created (%u vertices).",
                vkContext->gridVertexCount);
        }
    }

    // ── Create bounding box unit-cube wireframe vertex buffer ─────────────────
    // 8 corners of a unit cube at ±0.5, 12 edges → 24 line-segment endpoints.
    {
        // Corners

        // 12 edges, each as a pair of corner indices
        constexpr int edgeIndices[24] = {
            0,1, 1,2, 2,3, 3,0, // bottom face
            4,5, 5,6, 6,7, 7,4, // top face
            0,4, 1,5, 2,6, 3,7  // vertical pillars
        };

        std::vector<Vertex3D> boxVerts;
        boxVerts.reserve(24);
        for (int edgeIndice : edgeIndices)
        {
            const glm::vec3 p[8] = {
                { -0.5f, -0.5f, -0.5f }, // 0
                {  0.5f, -0.5f, -0.5f }, // 1
                {  0.5f,  0.5f, -0.5f }, // 2
                { -0.5f,  0.5f, -0.5f }, // 3
                { -0.5f, -0.5f,  0.5f }, // 4
                {  0.5f, -0.5f,  0.5f }, // 5
                {  0.5f,  0.5f,  0.5f }, // 6
                { -0.5f,  0.5f,  0.5f }, // 7
            };
            Vertex3D v{};
            v.position = p[edgeIndice];
            boxVerts.push_back(v);
        }

        vkContext->boundingBoxVertexCount = static_cast<uint32>(boxVerts.size());
        const uint64 bbBufSize = boxVerts.size() * sizeof(Vertex3D);

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, bbBufSize,
            static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, &vkContext->boundingBoxVertexBuffer))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[Initialize] Failed to create bounding box vertex buffer.");
        }
        else
        {
            NOUS_VulkanBuffer::LoadData(vkContext, &vkContext->boundingBoxVertexBuffer,
                0, bbBufSize, 0, boxVerts.data());
            NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Bounding box vertex buffer created (%u vertices).",
                vkContext->boundingBoxVertexCount);
        }
    }

    // ── Create point light debug unit-sphere wireframe vertex buffer ──────────
    // 3 great-circle rings (XY, XZ, YZ planes) as LINE_LIST; scaled per-draw
    // via push-constant model matrix (translate + uniform scale).
    {
        constexpr uint32 k_RingSegments = 24;
        std::vector<Vertex3D> sphereVerts;
        sphereVerts.reserve(k_RingSegments * 2 * 3);

        for (uint32 plane = 0; plane < 3; ++plane)
        {
            for (uint32 i = 0; i < k_RingSegments; ++i)
            {
                const float a0 = (2.0f * glm::pi<float>() * static_cast<float>(i))     / static_cast<float>(k_RingSegments);
                const float a1 = (2.0f * glm::pi<float>() * static_cast<float>(i + 1)) / static_cast<float>(k_RingSegments);
                const float c0 = std::cos(a0), s0 = std::sin(a0);
                const float c1 = std::cos(a1), s1 = std::sin(a1);

                Vertex3D v0{}, v1{};
                switch (plane)
                {
                    case 0: // XY ring
                        v0.position = { c0, s0, 0.0f };
                        v1.position = { c1, s1, 0.0f };
                        break;
                    case 1: // XZ ring
                        v0.position = { c0, 0.0f, s0 };
                        v1.position = { c1, 0.0f, s1 };
                        break;
                    case 2: // YZ ring
                        v0.position = { 0.0f, c0, s0 };
                        v1.position = { 0.0f, c1, s1 };
                        break;
                    default:
                        break;
                }
                sphereVerts.push_back(v0);
                sphereVerts.push_back(v1);
            }
        }

        vkContext->pointLightSphereVertexCount = static_cast<uint32>(sphereVerts.size());
        const uint64 sphereBufSize = sphereVerts.size() * sizeof(Vertex3D);

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, sphereBufSize,
            static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, &vkContext->pointLightSphereVertexBuffer))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[Initialize] Failed to create point light sphere vertex buffer.");
        }
        else
        {
            NOUS_VulkanBuffer::LoadData(vkContext, &vkContext->pointLightSphereVertexBuffer,
                0, sphereBufSize, 0, sphereVerts.data());
            NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Point light debug sphere vertex buffer created (%u vertices).",
                vkContext->pointLightSphereVertexCount);
        }
    }

    // ── Create directional light debug pyramid vertex buffer ──────────────────
    // 8 edges (4 base + 4 apex-to-corner) = 16 line endpoints.
    // Apex at (0,-1.5,0); base corners at (±0.2, 0, ±0.2). Points in local -Y.
    {
        const glm::vec3 apex = { 0.0f, -1.5f,  0.0f };
        const glm::vec3 bbl  = {-0.2f,  0.0f, -0.2f };
        const glm::vec3 bbr  = { 0.2f,  0.0f, -0.2f };
        const glm::vec3 bfr  = { 0.2f,  0.0f,  0.2f };
        const glm::vec3 bfl  = {-0.2f,  0.0f,  0.2f };

        std::array<Vertex3D, 16> pyramidVerts{};
        auto setPos = [](Vertex3D& v, const glm::vec3& p) { v.position = p; };

        setPos(pyramidVerts[0],  bbl);  setPos(pyramidVerts[1],  bbr);
        setPos(pyramidVerts[2],  bbr);  setPos(pyramidVerts[3],  bfr);
        setPos(pyramidVerts[4],  bfr);  setPos(pyramidVerts[5],  bfl);
        setPos(pyramidVerts[6],  bfl);  setPos(pyramidVerts[7],  bbl);
        setPos(pyramidVerts[8],  apex); setPos(pyramidVerts[9],  bbl);
        setPos(pyramidVerts[10], apex); setPos(pyramidVerts[11], bbr);
        setPos(pyramidVerts[12], apex); setPos(pyramidVerts[13], bfr);
        setPos(pyramidVerts[14], apex); setPos(pyramidVerts[15], bfl);

        vkContext->dirLightPyramidVertexCount = static_cast<uint32>(pyramidVerts.size());
        const uint64 bufSize = pyramidVerts.size() * sizeof(Vertex3D);

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, bufSize,
            static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, &vkContext->dirLightPyramidVertexBuffer))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[Initialize] Failed to create directional light pyramid vertex buffer.");
        }
        else
        {
            NOUS_VulkanBuffer::LoadData(vkContext, &vkContext->dirLightPyramidVertexBuffer,
                0, bufSize, 0, pyramidVerts.data());
            NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Dir light debug pyramid vertex buffer created (%u vertices).",
                vkContext->dirLightPyramidVertexCount);
        }
    }

    // ── Create spot light debug cone vertex buffer ─────────────────────────────
    // Apex at origin, base circle at y=-1 in XZ plane; 24 segments.
    // 24 base segments × 2 + 24 spokes × 2 = 96 line endpoints. Points in local -Y.
    {
        constexpr uint32 k_ConeSegments = 24;
        std::vector<Vertex3D> coneVerts;
        coneVerts.reserve(k_ConeSegments * 4);

        for (uint32 i = 0; i < k_ConeSegments; ++i)
        {
            const float a0 = (2.0f * glm::pi<float>() * static_cast<float>(i))     / static_cast<float>(k_ConeSegments);
            const float a1 = (2.0f * glm::pi<float>() * static_cast<float>(i + 1)) / static_cast<float>(k_ConeSegments);

            Vertex3D v0{}, v1{};
            v0.position = { std::cos(a0), -1.0f, std::sin(a0) };
            v1.position = { std::cos(a1), -1.0f, std::sin(a1) };
            coneVerts.push_back(v0);
            coneVerts.push_back(v1);

            Vertex3D vApex{}, vRim{};
            vApex.position = { 0.0f, 0.0f, 0.0f };
            vRim.position  = { std::cos(a0), -1.0f, std::sin(a0) };
            coneVerts.push_back(vApex);
            coneVerts.push_back(vRim);
        }

        vkContext->spotLightConeVertexCount = static_cast<uint32>(coneVerts.size());
        const uint64 bufSize = coneVerts.size() * sizeof(Vertex3D);

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, bufSize,
            static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, &vkContext->spotLightConeVertexBuffer))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[Initialize] Failed to create spot light cone vertex buffer.");
        }
        else
        {
            NOUS_VulkanBuffer::LoadData(vkContext, &vkContext->spotLightConeVertexBuffer,
                0, bufSize, 0, coneVerts.data());
            NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Spot light debug cone vertex buffer created (%u vertices).",
                vkContext->spotLightConeVertexCount);
        }
    }

    // ── Create camera frustum wireframe vertex buffer (dynamic, host-visible) ─
    // Capacity: 8 frustums × 24 vertices (12 edges × 2 endpoints per frustum).
    {
        constexpr uint32 k_MaxCameraFrustums    = 8;
        constexpr uint32 k_FrustumVertCapacity  = k_MaxCameraFrustums * 24;
        constexpr uint64 frustumBufSize = k_FrustumVertCapacity * sizeof(Vertex3D);

        if (!NOUS_VulkanBuffer::CreateBuffer(vkContext, frustumBufSize,
            static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, &vkContext->frustumVertexBuffer))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[Initialize] Failed to create camera frustum vertex buffer.");
        }
        else
        {
            vkContext->frustumVertexCapacity = k_FrustumVertCapacity;
            NOUS_INFO_C(CURRENT_CHANNEL, "[Initialize] Camera frustum vertex buffer created (capacity %u vertices).",
                k_FrustumVertCapacity);
        }
    }

	return ret;
}

void VulkanBackend::SetRenderMode(const RenderMode mode) noexcept
{
    vkContext->renderMode = mode;
}

void VulkanBackend::Shutdown() noexcept
{
    // Ensure frame resources are released before tearing down infrastructure.
    // No-op if ReleaseFrameResources() was already called by the Editor or Renderer.
    ReleaseFrameResources();

    // In GAME mode, the Editor never runs, so the descriptor pool and any other
    // ImGui/descriptor resources must be cleaned up here before the device is destroyed.
    // In EDITOR mode, ModuleEditor::CleanUp() already handled this.
    if (vkContext->renderMode == RenderMode::GAME)
        NOUS_ImGuiVulkanResources::DestroyImGuiVulkanResources(vkContext);

    NOUS_VulkanBuffer::DestroyBuffers(vkContext);

    // builtInMaterialShader is managed by the ResourceManager;
    // ClearResources() → DestroyShader() releases its GPU resources and nulls this
    // pointer before Shutdown() is called. Guard against the unexpected case.
    if (vkContext->builtInMaterialShader)
        NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInMaterialShader pointer still set — ResourceManager may not have cleared resources.");
    vkContext->builtInMaterialShader = nullptr;

    // In EDITOR mode, builtInGameShader is a VulkanBackend-owned clone (not in ResourceManager).
    // In GAME mode, it is ResourceManager-owned and will be released by ClearResources().
    if (vkContext->renderMode == RenderMode::EDITOR)
    {
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

        if (vkContext->builtInGameBackgroundShader)
        {
            if (vkContext->builtInGameBackgroundShader->internalData)
            {
                vkContext->builtInGameBackgroundShader->internalData->Destroy();
                vkContext->builtInGameBackgroundShader->internalData = nullptr;
            }
            NOUS_DELETE(vkContext->builtInGameBackgroundShader, MemoryTag::RESOURCE_SHADER);
            vkContext->builtInGameBackgroundShader = nullptr;
        }
    }
    else
    {
        // GAME mode: ResourceManager-owned; guard against unexpected state.
        if (vkContext->builtInGameShader)
            NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInGameShader pointer still set — ResourceManager may not have cleared resources.");
        vkContext->builtInGameShader = nullptr;

        if (vkContext->builtInGameBackgroundShader)
            NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInGameBackgroundShader pointer still set — ResourceManager may not have cleared resources.");
        vkContext->builtInGameBackgroundShader = nullptr;
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

    // builtInGridShader is managed by the ResourceManager (like builtInMaterialShader).
    if (vkContext->builtInGridShader)
        NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInGridShader pointer still set — ResourceManager may not have cleared resources.");
    vkContext->builtInGridShader = nullptr;

    // builtInSceneBackgroundShader is ResourceManager-owned.
    if (vkContext->builtInSceneBackgroundShader)
        NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInSceneBackgroundShader pointer still set — ResourceManager may not have cleared resources.");
    vkContext->builtInSceneBackgroundShader = nullptr;

    // Destroy the editor grid vertex buffer (not managed by ResourceManager).
    if (vkContext->gridVertexBuffer.handle != VK_NULL_HANDLE)
    {
        NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->gridVertexBuffer);
        vkContext->gridVertexBuffer.handle = VK_NULL_HANDLE;
        vkContext->gridVertexCount = 0;
    }

    // builtInBoundingBoxShader is ResourceManager-owned; guard like grid shader.
    if (vkContext->builtInBoundingBoxShader)
        NOUS_WARN_C(CURRENT_CHANNEL, "[Shutdown] builtInBoundingBoxShader pointer still set — ResourceManager may not have cleared resources.");
    vkContext->builtInBoundingBoxShader = nullptr;

    // Destroy the bounding box unit-cube vertex buffer (not managed by ResourceManager).
    if (vkContext->boundingBoxVertexBuffer.handle != VK_NULL_HANDLE)
    {
        NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->boundingBoxVertexBuffer);
        vkContext->boundingBoxVertexBuffer.handle = VK_NULL_HANDLE;
        vkContext->boundingBoxVertexCount = 0;
    }

    // Destroy the camera frustum vertex buffer.
    if (vkContext->frustumVertexBuffer.handle != VK_NULL_HANDLE)
    {
        NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->frustumVertexBuffer);
        vkContext->frustumVertexBuffer.handle = VK_NULL_HANDLE;
        vkContext->frustumVertexCapacity = 0;
    }

    // Destroy the point light debug sphere vertex buffer (not managed by ResourceManager).
    if (vkContext->pointLightSphereVertexBuffer.handle != VK_NULL_HANDLE)
    {
        NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->pointLightSphereVertexBuffer);
        vkContext->pointLightSphereVertexBuffer.handle = VK_NULL_HANDLE;
        vkContext->pointLightSphereVertexCount = 0;
    }

    if (vkContext->dirLightPyramidVertexBuffer.handle != VK_NULL_HANDLE)
    {
        NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->dirLightPyramidVertexBuffer);
        vkContext->dirLightPyramidVertexBuffer.handle = VK_NULL_HANDLE;
        vkContext->dirLightPyramidVertexCount = 0;
    }

    if (vkContext->spotLightConeVertexBuffer.handle != VK_NULL_HANDLE)
    {
        NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->spotLightConeVertexBuffer);
        vkContext->spotLightConeVertexBuffer.handle = VK_NULL_HANDLE;
        vkContext->spotLightConeVertexCount = 0;
    }

    // Destroy instance SSBOs.
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (vkContext->instanceSSBO[i].handle != VK_NULL_HANDLE)
        {
            vkUnmapMemory(vkContext->device.logicalDevice, vkContext->instanceSSBO[i].memory);
            vkContext->instanceSSBOMapped[i] = nullptr;
            NOUS_VulkanBuffer::DestroyBuffer(vkContext, &vkContext->instanceSSBO[i]);
        }
    }

    // Sync objects, renderpasses, swapchain and the logical device only exist when
    // Initialize() got past device creation. Skip them on a partial-init teardown
    // (failure before the logical device) so we don't pass a VK_NULL_HANDLE device
    // into the destroy helpers. Surface/debug-messenger/instance below were already
    // created and are always torn down.
    if (vkContext->device.logicalDevice != VK_NULL_HANDLE)
    {
        NOUS_VulkanSyncObjects::DestroySyncObjects(vkContext);

        if (vkContext->renderMode == RenderMode::GAME)
        {
            NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->gameSwapchainRenderpass);
        }
        else
        {
            NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->uiRenderpass);
            NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->gameRenderpass);
            NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->pickRenderpass);
            NOUS_VulkanRenderpass::DestroyRenderpass(vkContext, &vkContext->sceneRenderpass);
        }

        NOUS_VulkanSwapChain::DestroySwapChain(vkContext, &vkContext->swapChain);

        NOUS_VulkanDevice::DestroyLogicalDevice(vkContext);
    }

    NOUS_VulkanInstance::DestroySurface(vkContext);

    NOUS_VulkanDebugMessenger::DestroyDebugUtilsMessengerEXT(vkContext->instance, vkContext->debugMessenger, vkContext->allocator);

    NOUS_VulkanInstance::DestroyInstance(vkContext);
}

void VulkanBackend::ReleaseFrameResources() noexcept
{
    if (m_frameResourcesReleased) return;

    // Nothing to release if the logical device was never created (e.g. Initialize()
    // aborted at instance/surface/physical-device selection). Calling vkDeviceWaitIdle
    // with a VK_NULL_HANDLE device is invalid and would crash during failure-path teardown.
    if (vkContext->device.logicalDevice == VK_NULL_HANDLE)
    {
        m_frameResourcesReleased = true;
        return;
    }

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

void VulkanBackend::WaitForGPUIdle() noexcept
{
    vkDeviceWaitIdle(vkContext->device.logicalDevice);
}

void VulkanBackend::Resized(uint16 width, uint16 height) noexcept
{
    // Update the "framebuffer size generation", a counter which indicates when the
    // framebuffer size has been updated.

    m_cachedFramebufferWidth  = width;
    m_cachedFramebufferHeight = height;

    vkContext->framebufferSizeGeneration++;

    NOUS_INFO_C(CURRENT_CHANNEL, "Vulkan Renderer Backend --> Resized: W / H / GEN: %i / %i / %llu", width, height, vkContext->framebufferSizeGeneration);
}

FrameResult VulkanBackend::BeginFrame(float dt)
{
    ProcessPendingSubmissions();

    vkContext->frameDeltaTime = dt;
    const VulkanDevice* device = &vkContext->device;

    // If we are in the middle of recreating the swapchain, attempt to rebuild now.
    if (vkContext->recreatingSwapchain)
    {
        // Clear the flag BEFORE calling RecreateResources() — otherwise RecreateResources()
        // would immediately return false seeing the flag still set.
        vkContext->recreatingSwapchain = false;

        if (const VkResult waitRes = vkDeviceWaitIdle(device->logicalDevice); !VkResultIsSuccess(waitRes))
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
#ifdef _PROFILING
        ZoneScopedN("vkWaitForFences");
#endif
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
#ifdef _PROFILING
        ZoneScopedN("vkAcquireNextImageKHR");
#endif
        VkResult acquireRes = NOUS_VulkanSwapChain::SwapChainAcquireNextImageIndex(
                vkContext,
                &vkContext->swapChain,
                UINT64_MAX,
                vkContext->imageAvailableSemaphores[vkContext->currentFrame],
                VK_NULL_HANDLE,
                &vkContext->imageIndex);

        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Acquire FAILED: no image was acquired and imageAvailableSemaphores[currentFrame]
            // was NOT signaled. Safe to schedule recreation and skip now.
            vkContext->recreatingSwapchain = true;
            NOUS_INFO_C(CURRENT_CHANNEL, "Swapchain acquire returned %s. Scheduling recreation, skipping frame.",
                      VkResultMessage(acquireRes, true).c_str());
            return FrameResult::SKIPPED;
        }

        // VK_SUBOPTIMAL_KHR is NOT handled here: on acquire it means the image WAS acquired and
        // the semaphore IS signaled. Skipping would leak that signaled semaphore (a later acquire
        // on this frame slot would then try to signal an already-signaled semaphore). Instead we
        // render and present this frame normally; the suboptimal-on-present path in EndFrame (or a
        // resize) drives recreation, which leaves the semaphore correctly consumed by the submit.
        if (acquireRes != VK_SUBOPTIMAL_KHR && !VkResultIsSuccess(acquireRes))
        {
            NOUS_FATAL_C(CURRENT_CHANNEL, "Failed to acquire next image index: %s", VkResultMessage(acquireRes, true).c_str());
            return FrameResult::ERROR;
        }
    }

    // Wait for any previous frame that was rendering to this swapchain image to finish.
    // Must happen here — BEFORE BeginRenderpass resets/records the command buffer for imageIndex.
    if (vkContext->imagesInFlight[vkContext->imageIndex] != VK_NULL_HANDLE)
    {
#ifdef _PROFILING
        ZoneScopedN("vkWaitForFences (image-in-flight)");
#endif
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

    // Collect command buffers for this frame.
    // GAME mode: only the game CB (direct swapchain).
    // EDITOR mode: scene viewport + game viewport + UI/ImGui.
    VkCommandBuffer gameCB = vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex].handle;

    // Declared here so it outlives the branch and pCommandBuffers never dangles.
    std::array<VkCommandBuffer, 3> cmdBuffersEditor = {};

    // Submit to graphics queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    if (vkContext->renderMode == RenderMode::GAME)
    {
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &gameCB;
    }
    else
    {
        // Only access EDITOR-only vectors inside this branch.
        cmdBuffersEditor =
        {
            vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex].handle,
            gameCB,
            vkContext->graphicsCommandBuffers[vkContext->imageIndex].handle
        };
        submitInfo.commandBufferCount = static_cast<uint32>(cmdBuffersEditor.size());
        submitInfo.pCommandBuffers    = cmdBuffersEditor.data();
    }

    // Signal when graphics queue is done. Indexed by imageIndex (per swapchain image),
    // since this semaphore is the present wait semaphore below — see CreateSyncObjects.
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &vkContext->queueCompleteSemaphores[vkContext->imageIndex];

    // Wait on the "image available" semaphore before executing CBs
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = &vkContext->imageAvailableSemaphores[vkContext->currentFrame];

    // IMPORTANT: wait stage mask count MUST equal waitSemaphoreCount (was mismatched before)
    constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
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
    NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(
            &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex]);
    if (vkContext->renderMode == RenderMode::EDITOR)
    {
        NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(
                &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex]);
        NOUS_VulkanCommandBuffer::CommandBufferUpdateSubmitted(
                &vkContext->graphicsCommandBuffers[vkContext->imageIndex]);
    }

    // Present
    const VkResult presentRes = NOUS_VulkanSwapChain::SwapChainPresent(
            vkContext,
            &vkContext->swapChain,
            vkContext->device.graphicsQueue,
            vkContext->device.presentQueue,
            vkContext->queueCompleteSemaphores[vkContext->imageIndex],
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
    VkFramebuffer framebuffer = nullptr;
    
    switch (renderpassID)
    {
        case RenderpassType::SCENE:
        {
            commandBuffer = &vkContext->imGuiResources.m_ViewportCommandBuffers[vkContext->imageIndex];
            renderpass = &vkContext->sceneRenderpass;
            framebuffer = vkContext->imGuiResources.m_ViewportFramebuffers[vkContext->imageIndex];
            // New scene command buffer this frame — no wireframe draw has bound set=0 yet.
            vkContext->wireframeGlobalSetThisFrame = false;
            break;
        }
        case RenderpassType::GAME:
        {
            commandBuffer = &vkContext->imGuiResources.m_GameViewportCommandBuffers[vkContext->imageIndex];
            if (vkContext->renderMode == RenderMode::GAME)
            {
                renderpass  = &vkContext->gameSwapchainRenderpass;
                framebuffer = vkContext->gameSwapchainFramebuffers[vkContext->imageIndex];
            }
            else
            {
                renderpass  = &vkContext->gameRenderpass;
                framebuffer = vkContext->imGuiResources.m_GameViewportFramebuffers[vkContext->imageIndex];
            }
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
    viewport.y = static_cast<float>(vkContext->framebufferHeight);

    viewport.width = static_cast<float>(vkContext->framebufferWidth);
    viewport.height = -static_cast<float>(vkContext->framebufferHeight);

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

    renderpass->renderArea.z = static_cast<float>(vkContext->framebufferWidth);
    renderpass->renderArea.w = static_cast<float>(vkContext->framebufferHeight);

    NOUS_VulkanRenderpass::BeginRenderpass(commandBuffer, renderpass, framebuffer);

    // Initial pipeline bind; UpdateGlobalWorldState / DrawGeometry will re-bind as needed.
    auto TryBind = [&](const ResourceShader* rs) {
        if (rs && rs->internalData)
            NOUS_VulkanShader::BindPipeline(commandBuffer->handle,
                down_cast<VulkanShader*>(rs->internalData));
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
            renderpass = vkContext->renderMode == RenderMode::GAME
                ? &vkContext->gameSwapchainRenderpass
                : &vkContext->gameRenderpass;
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
        vkContext->imagesInFlight[i] = nullptr;
    }

    // Requery support and depth format
    vkContext->device.swapChainSupport = NOUS_VulkanDevice::QuerySwapChainSupport(vkContext->device.physicalDevice, vkContext);
    vkContext->device.depthFormat = NOUS_VulkanDevice::FindDepthFormat(vkContext->device.physicalDevice);

    NOUS_VulkanSwapChain::RecreateSwapChain(vkContext, m_cachedFramebufferWidth, m_cachedFramebufferHeight, &vkContext->swapChain);

    // CreateSwapChain set framebufferWidth/Height to the *actual* surface extent (which may
    // differ from the requested cached size). Use that — sizing framebuffers/offscreen images
    // to the requested size instead would mismatch the swapchain images (VUID-...-04534).
    vkContext->framebufferWidth  = static_cast<int32>(vkContext->swapChain.swapChainExtent.width);
    vkContext->framebufferHeight = static_cast<int32>(vkContext->swapChain.swapChainExtent.height);

    vkContext->eventSystem->Broadcast(Event(EventType::IMGUI_RECREATION, {}));

    m_cachedFramebufferWidth  = 0;
    m_cachedFramebufferHeight = 0;

    // Update framebuffer size generation.
    vkContext->framebufferSizeLastGeneration = vkContext->framebufferSizeGeneration;

    // Free old command buffers and framebuffers before recreating them.
    if (vkContext->renderMode == RenderMode::GAME)
    {
        for (uint32 i = 0; i < vkContext->swapChain.swapChainImages.size(); ++i)
        {
            NOUS_VulkanCommandBuffer::CommandBufferFree(vkContext, vkContext->device.mainGraphicsCommandPool,
                &vkContext->imGuiResources.m_GameViewportCommandBuffers[i]);
        }
        for (uint32 i = 0; i < vkContext->gameSwapchainFramebuffers.size(); ++i)
        {
            if (vkContext->gameSwapchainFramebuffers[i])
            {
                vkDestroyFramebuffer(vkContext->device.logicalDevice, vkContext->gameSwapchainFramebuffers[i], vkContext->allocator);
                vkContext->gameSwapchainFramebuffers[i] = VK_NULL_HANDLE;
            }
        }
        vkContext->gameSwapchainRenderpass.renderArea = { 0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight };
    }
    else
    {
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
    }

    // Regenerate world framebuffers
    NOUS_VulkanFramebuffer::CreateFramebuffers(vkContext);

    NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext);

    // Clear the recreating flag.
    vkContext->recreatingSwapchain = false;

    return true;
}

bool VulkanBackend::UpdateGlobalWorldState(
        const RenderpassType renderpassID,
        const GlobalUBO& globalUBO)
{
#ifdef _PROFILING
    ZoneScopedN("UpdateGlobalWorldState");
#endif
    const ResourceShader* rShader = (renderpassID == RenderpassType::GAME)
        ? vkContext->builtInGameShader
        : vkContext->builtInMaterialShader;

    if (!rShader || !rShader->internalData) return false;

    const VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    auto* vs = down_cast<VulkanShader*>(rShader->internalData);

    NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vs);
    NOUS_VulkanShader::UpdateGlobal(vkContext, commandBuffer->handle, vs,
        vkContext->imageIndex, &globalUBO, sizeof(GlobalUBO));

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

// ── Uniform-value helpers for data-driven InstanceUBO upload ───────────────
// Write a UniformValue into the UBO byte buffer at the given offset.
// Callers must have already bounds-checked `offset`.
static void WriteUniformValueToBuffer(uint8_t* dst, const UniformValue& uv)
{
    const uint32_t count = UniformValueComponentCount(uv.type);
    if (UniformValueIsInt(uv.type))
        std::memcpy(dst, &uv.idata, count * sizeof(int32_t));
    else
        std::memcpy(dst, &uv.fdata, count * sizeof(float));
}

bool VulkanBackend::DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData)
{
#ifdef _PROFILING
    ZoneScopedN("DrawGeometry");
#endif
    if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID)
        return true;

    // The base shader owns the instance pool and descriptor state.
    ResourceShader* baseShader = (renderpassID == RenderpassType::GAME)
        ? vkContext->builtInGameShader
        : vkContext->builtInMaterialShader;

    if (!baseShader || !baseShader->internalData) return false;

    // Custom shader pipeline: compiled against sceneRenderpass, which is structurally
    // identical to gameRenderpass (same format/sample count) — Vulkan compatible in both.
    ResourceShader* drawShader = baseShader;
    if (renderData.material != nullptr &&
        renderData.material->shader != nullptr &&
        renderData.material->shader->internalData != nullptr &&
        renderData.material->shader->GetState() == ResourceState::GPU_READY)
    {
        drawShader = renderData.material->shader;
    }

    const VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    auto* vsBase = down_cast<VulkanShader*>(baseShader->internalData);
    auto* vsDraw = down_cast<VulkanShader*>(drawShader->internalData);
    const VulkanGeometryData* bufferData = &vkContext->geometries[renderData.geometry->internalID];

    // Bind pipeline — use custom shader's VkPipeline if assigned, else base shader.
    NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vsDraw);

    // Push model matrix via push constants — use base shader's layout (compatible with draw shader).
    vkCmdPushConstants(commandBuffer->handle, vsBase->pipeline.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &renderData.model);

    // Resolve material.
    // Fall back to the default if the material is null or has not been uploaded to the GPU yet
    // (internalID == INVALID_ID means CreateMaterial hasn't run for this resource).
    // This handles the one-frame window where RegisterGameObject runs on a job thread
    // after Renderer::PreUpdate has already drained the pending-upload queue — the
    // material sits in CPU_READY state until the *next* PreUpdate uploads it.
    // Without this guard, DrawGeometry would skip BindInstanceDescriptorSet while the
    // pipeline still statically uses set 1, triggering VUID-vkCmdDrawIndexed-None-08600.
    ResourceMaterial* material = renderData.material;
    if (!material || material->internalID == INVALID_ID)
        material = vkContext->resourceManager->GetDefaultMaterial();

    // vsInstance is the shader whose instance pool holds this material's descriptor slot.
    // Use poolOwnerShader as the authoritative source — it was set at CreateMaterial time
    // and reflects exactly which pool this material's internalID slot was acquired from.
    // Falling back to heuristics (e.g. vsDraw->instancePool) gives the wrong answer when
    // the reslot fires before the custom shader is GPU_READY, causing a NULL descriptor set.
    VulkanShader* vsInstance = vsBase; // safe default
    if (material && material->poolOwnerShader && material->poolOwnerShader->internalData)
        vsInstance = down_cast<VulkanShader*>(material->poolOwnerShader->internalData);

    // Even the default material isn't GPU-ready yet, or the instance pool isn't initialised.
    // Skip the draw — issuing it would leave set #1 unbound and trigger VUID-vkCmdDrawIndexed-None-08600.
    if (!material || material->internalID == INVALID_ID || !vsInstance->instancePool)
        return true;

    // Per-instance descriptors (material UBO + texture sampler).
    // All descriptor state is owned by vsBase (the instance pool owner).
    {
        const uint32_t instanceID  = material->internalID;
        const uint32_t imageIndex  = vkContext->imageIndex;

        // Write per-instance params to UBO (binding 0) via shader reflection.
        // The byte buffer is sized to the reflected block and populated from
        // `material->uniformValues` — no hardcoded struct layout.
        const ReflectedBinding* instanceUBOBinding = nullptr;
        {
            const auto& setIt = drawShader->reflection.descriptorSets.find(1);
            if (setIt != drawShader->reflection.descriptorSets.end())
            {
                for (const auto& rb : setIt->second)
                {
                    if (rb.type == DescriptorType::UniformBuffer && rb.binding == 0)
                    {
                        instanceUBOBinding = &rb;
                        break;
                    }
                }
            }
        }

        if (instanceUBOBinding && instanceUBOBinding->blockSize > 0)
        {
            const uint32_t blockSize = instanceUBOBinding->blockSize;

            // Stack-allocate for small UBOs (typical: 48-256 bytes), heap for large.
            constexpr uint32_t c_stackThreshold = 1024;
            uint8_t stackBuffer[c_stackThreshold];
            std::unique_ptr<uint8_t[]> heapBuffer;
            uint8_t* uboBuffer = stackBuffer;
            if (blockSize > c_stackThreshold)
            {
                heapBuffer = std::make_unique<uint8_t[]>(blockSize);
                uboBuffer  = heapBuffer.get();
            }

            // Zero-fill. Per-member defaults are applied below for any member
            // the material has no entry for, using type-correct bit patterns.
            std::memset(uboBuffer, 0, blockSize);

            for (const auto& member : instanceUBOBinding->members)
            {
                const uint32_t memberBytes = [&]() -> uint32_t {
                    switch (member.type)
                    {
                        case DataType::Float: case DataType::Int: return 4;
                        case DataType::Vec2:  case DataType::IVec2: return 8;
                        case DataType::Vec3:  case DataType::IVec3: return 12;
                        case DataType::Vec4:  case DataType::IVec4: return 16;
                        default: return 0;
                    }
                }();

                if (memberBytes == 0 || member.offset + memberBytes > blockSize)
                    continue;

                auto it = material->uniformValues.find(member.name);
                if (it != material->uniformValues.end())
                {
                    WriteUniformValueToBuffer(uboBuffer + member.offset, it->second);
                }
                else
                {
                    const UniformValue fallback =
                        UniformValue::MakeDefault(DataTypeToUniformValueType(member.type));
                    WriteUniformValueToBuffer(uboBuffer + member.offset, fallback);
                }
            }

            auto& uboGen = vsInstance->instanceStates[instanceID].descriptorStates[0].generations[imageIndex];
            NOUS_VulkanShader::WriteInstanceUBO(vkContext, vsInstance, imageIndex, instanceID,
                uboBuffer, blockSize, &uboGen);
        }

        // Write sampler bindings from baseShader reflection — no hardcoded binding indices.
        // For each CombinedImageSampler in set=1, look up the texture by binding name from
        // the material's textureMaps, falling back to the default texture if missing.
        {
            const auto& setIt = drawShader->reflection.descriptorSets.find(1);
            if (setIt != drawShader->reflection.descriptorSets.end())
            {
                // Sort samplers by binding index to match the descriptor layout order.
                std::vector<const ReflectedBinding*> samplers;
                for (const auto& rb : setIt->second)
                    if (rb.type == DescriptorType::CombinedImageSampler)
                        samplers.push_back(&rb);
                std::ranges::sort(samplers,
                                  [](const ReflectedBinding* a, const ReflectedBinding* b){
                                      return a->binding < b->binding; });

                for (const ReflectedBinding* rb : samplers)
                {
                    ResourceTexture* texture = nullptr;
                    if (auto texIt = material->textureMaps.find(rb->name); texIt != material->textureMaps.end())
                        texture = texIt->second.texture;

                    if (!texture || texture->generation == INVALID_ID || !texture->internalData)
                    {
                        // Each slot falls back to a semantically neutral texture so unassigned
                        // maps have no visible effect on the final result.
                        if (const std::string& n = rb->name; n.find("diffuse") != std::string::npos ||
                            n.find("Diffuse") != std::string::npos ||
                            n.find("albedo")  != std::string::npos ||
                            n.find("Albedo")  != std::string::npos)
                        {
                            // Checkerboard — visible "missing texture" signal.
                            texture = vkContext->resourceManager->GetDefaultTexture();
                        }
                        else if (n.find("normal") != std::string::npos ||
                                 n.find("Normal") != std::string::npos)
                        {
                            // (128,128,255) decodes to tangent-space (0,0,1) → geometry normal passthrough.
                            texture = vkContext->resourceManager->GetFlatNormalTexture();
                        }
                        else if (n.find("emissive") != std::string::npos ||
                                 n.find("Emissive") != std::string::npos)
                        {
                            // Black → adds nothing to the final colour.
                            texture = vkContext->resourceManager->GetBlackTexture();
                        }
                        else
                        {
                            // White → multiplicative identity (specular strength = 1,
                            // shininess = max, AO = 1 / no occlusion).
                            texture = vkContext->resourceManager->GetWhiteTexture();
                        }
                    }

                    if (texture && texture->internalData)
                    {
                        const auto* texData = static_cast<VulkanTextureData*>(texture->internalData);
                        auto& samplerGen = vsInstance->instanceStates[instanceID].descriptorStates[rb->binding].generations[imageIndex];
                        auto& samplerID  = vsInstance->instanceStates[instanceID].descriptorStates[rb->binding].ids[imageIndex];
                        NOUS_VulkanShader::WriteInstanceSampler(vkContext, vsInstance, imageIndex, instanceID,
                            rb->binding, texData->image.view, texData->sampler,
                            &samplerGen, &samplerID, texture->GetUID(), texture->generation);
                    }
                    else if (vsInstance->instanceStates[instanceID].descriptorStates[rb->binding].generations[imageIndex] == UINT32_MAX)
                    {
                        // This binding has never been written and no valid texture is available.
                        // Drawing now would violate VUID-vkCmdDrawIndexed-None-08114 — skip.
                        return true;
                    }
                }
            }
        }

        // When the material's slot lives in vsDraw's own pool (poolOwnerShader == drawShader),
        // vsInstance == vsDraw and the descriptor set layout already matches the pipeline — no override.
        // When the slot lives in a different pool (e.g. vsBase fallback), override to vsDraw's
        // pipeline layout so Vulkan uses the correct layout for the bind point.
        VkPipelineLayout bindLayout = (vsInstance != vsDraw)
                                            ? vsDraw->pipeline.pipelineLayout
                                            : VK_NULL_HANDLE;
        NOUS_VulkanShader::BindInstanceDescriptorSet(commandBuffer->handle, vsInstance, imageIndex, instanceID, bindLayout);
    }

    // Bind vertex buffer.
    const VkDeviceSize offset = bufferData->vertexBufferOffset;
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

void VulkanBackend::UploadInstanceMatrices(const glm::mat4* matrices,
                                            uint32_t count,
                                            uint32_t instanceOffset)
{
    if (count == 0 || !matrices) return;
    const uint32_t capacity  = 2 * c_maxInstances;
    const uint32_t safeCount = (instanceOffset < capacity)
                             ? std::min(count, capacity - instanceOffset)
                             : 0;
    if (safeCount == 0) return;

    // Select the SSBO ring slot by swapchain image index, matching the static binding
    // globalDescriptorSets[i] -> instanceSSBO[i % 3] (WriteInstanceSSBODescriptor). This makes
    // the buffer the draw reads identical to the one we write — unlike indexing by a CPU frame
    // counter, which desyncs from imageIndex after a swapchain recreate (currentFrame resets to 0
    // while the frame counter keeps advancing) and made meshes read stale matrices post-resize.
    const uint32_t slot = vkContext->imageIndex % 3;
    auto* base = static_cast<glm::mat4*>(vkContext->instanceSSBOMapped[slot]);
    std::memcpy(base + instanceOffset, matrices, safeCount * sizeof(glm::mat4));
}

bool VulkanBackend::DrawGeometryBatched(RenderpassType renderpassID,
                                         const InstancedBatch& batch)
{
#ifdef _PROFILING
    ZoneScopedN("DrawGeometryBatched");
#endif
    if (!batch.geometry || batch.geometry->internalID == INVALID_ID || batch.instanceCount == 0)
        return true;

    // The base shader owns the instance pool and global descriptor sets.
    ResourceShader* baseShader = (renderpassID == RenderpassType::GAME)
        ? vkContext->builtInGameShader
        : vkContext->builtInMaterialShader;

    if (!baseShader || !baseShader->internalData) return false;

    // Custom shader pipeline: use material's shader if GPU-ready; otherwise fall back to base.
    ResourceShader* drawShader = baseShader;
    if (batch.material != nullptr &&
        batch.material->shader != nullptr &&
        batch.material->shader->internalData != nullptr &&
        batch.material->shader->GetState() == ResourceState::GPU_READY)
    {
        drawShader = batch.material->shader;
    }

    const VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    auto* vsBase = down_cast<VulkanShader*>(baseShader->internalData);
    auto* vsDraw = down_cast<VulkanShader*>(drawShader->internalData);
    const VulkanGeometryData* bufferData = &vkContext->geometries[batch.geometry->internalID];

    // Bind pipeline.
    NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vsDraw);

    // Resolve material (same fallback logic as DrawGeometry).
    ResourceMaterial* material = batch.material;
    if (!material || material->internalID == INVALID_ID)
        material = vkContext->resourceManager->GetDefaultMaterial();

    VulkanShader* vsInstance = vsBase;
    if (material && material->poolOwnerShader && material->poolOwnerShader->internalData)
        vsInstance = down_cast<VulkanShader*>(material->poolOwnerShader->internalData);

    if (!material || material->internalID == INVALID_ID || !vsInstance->instancePool)
        return true;

    const uint32_t instanceID = material->internalID;
    const uint32_t imageIndex = vkContext->imageIndex;

    // Write per-instance UBO (same as DrawGeometry).
    const ReflectedBinding* instanceUBOBinding = nullptr;
    {
        const auto& setIt = drawShader->reflection.descriptorSets.find(1);
        if (setIt != drawShader->reflection.descriptorSets.end())
        {
            for (const auto& rb : setIt->second)
            {
                if (rb.type == DescriptorType::UniformBuffer && rb.binding == 0)
                {
                    instanceUBOBinding = &rb;
                    break;
                }
            }
        }
    }

    if (instanceUBOBinding && instanceUBOBinding->blockSize > 0)
    {
        const uint32_t blockSize = instanceUBOBinding->blockSize;

        constexpr uint32_t c_stackThreshold = 1024;
        uint8_t stackBuffer[c_stackThreshold];
        std::unique_ptr<uint8_t[]> heapBuffer;
        uint8_t* uboBuffer = stackBuffer;
        if (blockSize > c_stackThreshold)
        {
            heapBuffer = std::make_unique<uint8_t[]>(blockSize);
            uboBuffer  = heapBuffer.get();
        }

        std::memset(uboBuffer, 0, blockSize);

        for (const auto& member : instanceUBOBinding->members)
        {
            const uint32_t memberBytes = [&]() -> uint32_t {
                switch (member.type)
                {
                    case DataType::Float: case DataType::Int: return 4;
                    case DataType::Vec2:  case DataType::IVec2: return 8;
                    case DataType::Vec3:  case DataType::IVec3: return 12;
                    case DataType::Vec4:  case DataType::IVec4: return 16;
                    default: return 0;
                }
            }();

            if (memberBytes == 0 || member.offset + memberBytes > blockSize)
                continue;

            auto it = material->uniformValues.find(member.name);
            if (it != material->uniformValues.end())
                WriteUniformValueToBuffer(uboBuffer + member.offset, it->second);
            else
            {
                const UniformValue fallback = UniformValue::MakeDefault(DataTypeToUniformValueType(member.type));
                WriteUniformValueToBuffer(uboBuffer + member.offset, fallback);
            }
        }

        auto& uboGen = vsInstance->instanceStates[instanceID].descriptorStates[0].generations[imageIndex];
        NOUS_VulkanShader::WriteInstanceUBO(vkContext, vsInstance, imageIndex, instanceID,
            uboBuffer, blockSize, &uboGen);
    }

    // Write sampler bindings (same as DrawGeometry).
    {
        const auto& setIt = drawShader->reflection.descriptorSets.find(1);
        if (setIt != drawShader->reflection.descriptorSets.end())
        {
            std::vector<const ReflectedBinding*> samplers;
            for (const auto& rb : setIt->second)
                if (rb.type == DescriptorType::CombinedImageSampler)
                    samplers.push_back(&rb);
            std::ranges::sort(samplers,
                              [](const ReflectedBinding* a, const ReflectedBinding* b){
                                  return a->binding < b->binding; });

            for (const ReflectedBinding* rb : samplers)
            {
                ResourceTexture* texture = nullptr;
                if (auto texIt = material->textureMaps.find(rb->name); texIt != material->textureMaps.end())
                    texture = texIt->second.texture;

                if (!texture || texture->generation == INVALID_ID || !texture->internalData)
                {
                    if (const std::string& n = rb->name; n.find("diffuse") != std::string::npos ||
                        n.find("Diffuse") != std::string::npos ||
                        n.find("albedo")  != std::string::npos ||
                        n.find("Albedo")  != std::string::npos)
                        texture = vkContext->resourceManager->GetDefaultTexture();
                    else if (n.find("normal") != std::string::npos || n.find("Normal") != std::string::npos)
                        texture = vkContext->resourceManager->GetFlatNormalTexture();
                    else if (n.find("emissive") != std::string::npos || n.find("Emissive") != std::string::npos)
                        texture = vkContext->resourceManager->GetBlackTexture();
                    else
                        texture = vkContext->resourceManager->GetWhiteTexture();
                }

                if (texture && texture->internalData)
                {
                    const auto* texData = static_cast<VulkanTextureData*>(texture->internalData);
                    auto& samplerGen = vsInstance->instanceStates[instanceID].descriptorStates[rb->binding].generations[imageIndex];
                    auto& samplerID  = vsInstance->instanceStates[instanceID].descriptorStates[rb->binding].ids[imageIndex];
                    NOUS_VulkanShader::WriteInstanceSampler(vkContext, vsInstance, imageIndex, instanceID,
                        rb->binding, texData->image.view, texData->sampler,
                        &samplerGen, &samplerID, texture->GetUID(), texture->generation);
                }
                else if (vsInstance->instanceStates[instanceID].descriptorStates[rb->binding].generations[imageIndex] == UINT32_MAX)
                {
                    return true;
                }
            }
        }
    }

    // Bind instance descriptor set.
    VkPipelineLayout bindLayout = (vsInstance != vsDraw)
                                        ? vsDraw->pipeline.pipelineLayout
                                        : VK_NULL_HANDLE;
    NOUS_VulkanShader::BindInstanceDescriptorSet(commandBuffer->handle, vsInstance, imageIndex, instanceID, bindLayout);

    // Bind vertex + index buffers.
    const VkDeviceSize offset = bufferData->vertexBufferOffset;
    vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1,
        &vkContext->objectVertexBuffer.handle, &offset);

    if (bufferData->indexCount > 0)
    {
        vkCmdBindIndexBuffer(commandBuffer->handle, vkContext->objectIndexBuffer.handle,
            bufferData->indexBufferOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer->handle,
                         bufferData->indexCount,
                         batch.instanceCount,
                         0, 0,
                         batch.firstInstance);
    }
    else
    {
        vkCmdDraw(commandBuffer->handle, bufferData->vertexCount,
                  batch.instanceCount, 0, batch.firstInstance);
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

    auto* textureData = static_cast<VulkanTextureData*>(texture->internalData);
    const VkDeviceSize imageSize = texture->width * texture->height * texture->channelCount;

    // NOTE: Assumes 8 bits per channel.
    constexpr VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM; // RGBA

    // Create a staging buffer and load data into it.
    constexpr VkBufferUsageFlagBits usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    constexpr VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VulkanBuffer stagingBuffer{};

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

    VulkanCommandBuffer tempCommandBuffer{};
    VkCommandPool pool = NOUS_VulkanMultithreading::GetThreadCommandPool(vkContext, std::this_thread::get_id());
    VkQueue queue = vkContext->device.transferQueue;

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

    const VkResult result = vkCreateSampler(vkContext->device.logicalDevice, &samplerCreateInfo, vkContext->allocator, &textureData->sampler);
    
    if (!VkResultIsSuccess(result)) 
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Error creating texture sampler: %s", VkResultMessage(result, true).c_str());
        return false;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Texture Created Successfully: %s", VkResultMessage(result, true).c_str());
    return true;
}

bool VulkanBackend::UpdateDynamicTexture(const uint8* pixels, ResourceTexture* texture)
{
    if (!texture || !texture->internalData || !pixels)
        return false;

    auto* textureData = static_cast<VulkanTextureData*>(texture->internalData);
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(texture->width) * texture->height * texture->channelCount;
    constexpr VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // Staging buffer with the new frame's pixels.
    constexpr VkBufferUsageFlagBits usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    constexpr VkMemoryPropertyFlags memoryPropertyFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VulkanBuffer stagingBuffer{};
    NOUS_VulkanBuffer::CreateBuffer(vkContext, imageSize, usage, memoryPropertyFlags, true, &stagingBuffer);
    NOUS_VulkanBuffer::LoadData(vkContext, &stagingBuffer, 0, imageSize, 0, pixels);

    VulkanCommandBuffer tempCommandBuffer{};
    VkCommandPool pool  = NOUS_VulkanMultithreading::GetThreadCommandPool(vkContext, std::this_thread::get_id());
    VkQueue       queue = vkContext->device.transferQueue;

    NOUS_VulkanCommandBuffer::CommandBufferAllocateAndBeginSingleTime(vkContext, pool, &tempCommandBuffer);

    // Existing image is in SHADER_READ_ONLY_OPTIMAL -> receive data -> back to shader-read.
    NOUS_VulkanImage::TransitionVulkanImageLayout(vkContext, &tempCommandBuffer, &textureData->image, imageFormat,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    NOUS_VulkanImage::CopyBufferToVulkanImage(vkContext, &textureData->image, stagingBuffer.handle, &tempCommandBuffer);

    NOUS_VulkanImage::TransitionVulkanImageLayout(vkContext, &tempCommandBuffer, &textureData->image, imageFormat,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    NOUS_VulkanCommandBuffer::CommandBufferEndAndFreeSingleTime(vkContext, pool, &tempCommandBuffer, queue);
    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &stagingBuffer);

    texture->generation++;   // keep monotonic (harmless; sampler binding is unchanged)
    return true;
}

void VulkanBackend::DestroyTexture(ResourceTexture* texture) noexcept
{
    auto* textureData = static_cast<VulkanTextureData*>(texture->internalData);

    if (textureData)
    {
        // Ensure no in-flight command buffers still reference this image/view before freeing.
        vkDeviceWaitIdle(vkContext->device.logicalDevice);

        NOUS_VulkanImage::DestroyVulkanImage(vkContext, &textureData->image);
        nous::engine::memory::ZeroMemory(&textureData->image, sizeof(VulkanImage));

        vkDestroySampler(vkContext->device.logicalDevice, textureData->sampler, vkContext->allocator);
        textureData->sampler = nullptr;

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

    // Resolve pool owner: use material's custom shader if it is GPU-ready and has a pool.
    // Falls back to the built-in material shader (or game shader in GAME mode).
    ResourceShader* poolOwner = nullptr;
    if (material->shader && material->shader->internalData &&
        material->shader->GetState() == ResourceState::GPU_READY)
    {
        const auto* vs = down_cast<VulkanShader*>(material->shader->internalData);
        if (vs->instancePool)
            poolOwner = material->shader;
    }
    if (!poolOwner)
        poolOwner = vkContext->builtInMaterialShader
                    ? vkContext->builtInMaterialShader
                    : vkContext->builtInGameShader;

    if (!poolOwner || !poolOwner->internalData)
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "VulkanBackend::CreateMaterial() — no shader/instance pool ready yet; will retry after shaders init.");
        return false;
    }

    auto* vs = down_cast<VulkanShader*>(poolOwner->internalData);
    uint32_t instanceID = 0;
    if (!NOUS_VulkanShader::AcquireInstanceSlot(vkContext, vs, &instanceID))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "VulkanBackend::CreateMaterial() - Instance pool full.");
        return false;
    }
    material->internalID      = instanceID;
    material->poolOwnerShader = poolOwner;

    // In EDITOR mode, also acquire a matching slot in the game shader clone so both
    // pools stay in sync — but only for materials using the built-in shader.
    // Custom-shader materials are drawn directly from their own pool in both renderpasses.
    if (poolOwner == vkContext->builtInMaterialShader
        && vkContext->builtInGameShader && vkContext->builtInGameShader->internalData)
    {
        auto* vsGame = down_cast<VulkanShader*>(vkContext->builtInGameShader->internalData);
        uint32_t gameID = 0;
        NOUS_VulkanShader::AcquireInstanceSlot(vkContext, vsGame, &gameID);
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Material created (instance %u, pool: %s).",
                material->internalID, poolOwner->GetName().c_str());
    return true;
}

void VulkanBackend::DestroyMaterial(ResourceMaterial* material) noexcept
{
    if (!material)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "VulkanBackend::DestroyMaterial() called with nullptr. Nothing was done.");
        return;
    }

    if (material->internalID == INVALID_ID)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "VulkanBackend::DestroyMaterial() called with INVALID_ID.");
        return;
    }

    // Ensure descriptor sets are no longer referenced by any in-flight command buffers.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // Release from the owning pool.
    if (material->poolOwnerShader && material->poolOwnerShader->internalData)
    {
        auto* vs = down_cast<VulkanShader*>(material->poolOwnerShader->internalData);
        NOUS_VulkanShader::ReleaseInstanceSlot(vkContext, vs, material->internalID);
    }

    // Release game clone slot only for built-in shader materials (custom shaders share
    // one pool between scene and game renderpasses — no clone slot to release).
    if (material->poolOwnerShader == vkContext->builtInMaterialShader
        && vkContext->builtInGameShader && vkContext->builtInGameShader->internalData)
    {
        auto* vsGame = down_cast<VulkanShader*>(vkContext->builtInGameShader->internalData);
        NOUS_VulkanShader::ReleaseInstanceSlot(vkContext, vsGame, material->internalID);
    }

    material->internalID      = INVALID_ID;
    material->poolOwnerShader = nullptr;
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
    const bool isReupload = geometry->internalID != INVALID_ID;

    VulkanGeometryData oldRange{};
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

    VkCommandPool pool = NOUS_VulkanMultithreading::GetThreadCommandPool(vkContext, std::this_thread::get_id());
    VkQueue queue = vkContext->device.transferQueue;

    // Vertex data.
    internalData->vertexCount = vertexCount;
    internalData->vertexSize = sizeof(Vertex3D) * vertexCount;

    {
        std::lock_guard vtxLock(vkContext->vertexBufferMutex);
        if (!NOUS_VulkanBuffer::UploadDataRange(vkContext, pool, nullptr, queue, &vkContext->objectVertexBuffer,
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

        std::lock_guard idxLock(vkContext->indexBufferMutex);
        if (!NOUS_VulkanBuffer::UploadDataRange(vkContext, pool, nullptr, queue, &vkContext->objectIndexBuffer,
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
            std::lock_guard vtxLock(vkContext->vertexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectVertexBuffer,
                oldRange.vertexBufferOffset, oldRange.vertexSize);
        }

        // Free index data, if applicable
        if (oldRange.indexSize > 0)
        {
            std::lock_guard idxLock(vkContext->indexBufferMutex);
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
            std::lock_guard vtxLock(vkContext->vertexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectVertexBuffer, internalData->vertexBufferOffset, internalData->vertexSize);
        }

        // Free index data, if applicable
        if (internalData->indexSize > 0)
        {
            std::lock_guard idxLock(vkContext->indexBufferMutex);
            NOUS_VulkanBuffer::FreeDataRange(vkContext, &vkContext->objectIndexBuffer, internalData->indexBufferOffset, internalData->indexSize);
        }

        // Clean up data.
        {
            std::lock_guard geoLock(vkContext->geometriesMutex);
            nous::engine::memory::ZeroMemory(internalData, sizeof(VulkanGeometryData));
            internalData->ID = INVALID_ID;
            internalData->generation = INVALID_ID;
        }
    }
}

// ─────────────────────────────── Shaders ─────────────────────────────────

// Returns true if this shader declares the per-frame instance SSBO at set=0 binding=1.
static bool HasGlobalSSBOBinding(const ResourceShader* shader)
{
    auto it = shader->reflection.descriptorSets.find(0);
    if (it == shader->reflection.descriptorSets.end()) return false;
    for (const auto& rb : it->second)
        if (rb.binding == 1 && rb.type == DescriptorType::StorageBuffer)
            return true;
    return false;
}

// Writes the per-frame instance SSBO into set=0 binding=1 of every global descriptor set owned by vs.
// Must be called after AllocateGlobalResources (which creates globalDescriptorSets)
// AND after the instanceSSBO buffers are created.
static void WriteInstanceSSBODescriptor(VulkanContext* vkContext, VulkanShader* vs)
{
    const VkDeviceSize ssboSize = 2 * c_maxInstances * sizeof(glm::mat4);
    const VkDevice     dev      = vkContext->device.logicalDevice;
    const uint32_t     count    = static_cast<uint32_t>(vs->globalDescriptorSets.size());

    for (uint32_t i = 0; i < count; ++i)
    {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = vkContext->instanceSSBO[i % 3].handle;
        bufInfo.offset = 0;
        bufInfo.range  = ssboSize;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = vs->globalDescriptorSets[i];
        write.dstBinding      = 1;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }
}

// Convenience: checks reflection for SSBO binding then writes if present.
static void TryWriteInstanceSSBODescriptor(VulkanContext* vkContext, ResourceShader* shader)
{
    if (!shader || !shader->internalData || !HasGlobalSSBOBinding(shader)) return;
    WriteInstanceSSBODescriptor(vkContext, down_cast<VulkanShader*>(shader->internalData));
}

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
        VulkanRenderpass* gameRenderpassTarget = vkContext->renderMode == RenderMode::GAME
            ? &vkContext->gameSwapchainRenderpass
            : &vkContext->gameRenderpass;

        if (vkContext->renderMode == RenderMode::GAME)
        {
            // GAME mode: compile directly against swapchain renderpass; no scene clone needed.
            if (!NOUS_VulkanShader::Create(vkContext, gameRenderpassTarget, shader))
                return false;
            vkContext->builtInGameShader = shader;
            TryWriteInstanceSSBODescriptor(vkContext, shader);
            NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.MaterialShader assigned to gameSwapchainRenderpass (GAME mode).");
        }
        else
        {
            // EDITOR mode: primary → sceneRenderpass, clone → gameRenderpass.
            if (!NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader))
                return false;
            vkContext->builtInMaterialShader = shader;
            TryWriteInstanceSSBODescriptor(vkContext, shader);
            NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.MaterialShader assigned to sceneRenderpass.");

            auto* gameShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
            gameShader->stagesData = shader->stagesData;
            gameShader->reflection = shader->reflection;

            if (!NOUS_VulkanShader::Create(vkContext, gameRenderpassTarget, gameShader))
            {
                NOUS_WARN_C(CURRENT_CHANNEL, "[CreateShader] Failed to create game-renderpass variant; game viewport will be unavailable.");
                NOUS_DELETE(gameShader, MemoryTag::RESOURCE_SHADER);
            }
            else
            {
                vkContext->builtInGameShader = gameShader;
                TryWriteInstanceSSBODescriptor(vkContext, gameShader);
                NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.MaterialShader clone assigned to gameRenderpass.");
            }
        }

        return true;
    }

    // ── BuiltIn.PickShader → scene renderpass (for mouse picking) ──────────────
    //    Internal clone — not tracked by ResourceManager. Stored as builtInPickShader.
    if (assetPath.find("BuiltIn.PickShader") != std::string::npos)
    {
        auto* pickShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
        pickShader->stagesData = shader->stagesData;
        pickShader->reflection = shader->reflection;

        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->pickRenderpass, pickShader, {.disableBlending = true}))
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
                                        {.createOutlinePipelines = true}))
            return false;
        vkContext->builtInOutlineShader = shader;
        NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.OutlineShader assigned to sceneRenderpass.");
        return true;
    }

    // ── BuiltIn.GridShader → scene renderpass (editor grid overlay) ───────────
    //    Uses LINE_LIST topology. No game renderpass clone needed.
    if (assetPath.find("BuiltIn.GridShader") != std::string::npos)
    {
        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader,
                                        {.useLineTopology = true}))
            return false;
        vkContext->builtInGridShader = shader;
        NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.GridShader assigned to sceneRenderpass (LINE_LIST).");
        return true;
    }

    // ── BuiltIn.BackgroundShader → scene + game renderpass (viewport background) ─
    //    Fullscreen gradient with depth test OFF. Also creates a game renderpass clone.
    if (assetPath.find("BuiltIn.BackgroundShader") != std::string::npos)
    {
        VulkanRenderpass* gameRenderpassTarget = vkContext->renderMode == RenderMode::GAME
            ? &vkContext->gameSwapchainRenderpass
            : &vkContext->gameRenderpass;

        if (vkContext->renderMode == RenderMode::GAME)
        {
            // GAME mode: compile directly against swapchain renderpass; no scene clone needed.
            if (!NOUS_VulkanShader::Create(vkContext, gameRenderpassTarget, shader,
                                            {.noDepthTest = true}))
                return false;
            vkContext->builtInGameBackgroundShader = shader;
            NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.BackgroundShader assigned to gameSwapchainRenderpass (GAME mode).");
        }
        else
        {
            // EDITOR mode: primary → sceneRenderpass, clone → gameRenderpass.
            if (!NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader,
                                            {.noDepthTest = true}))
                return false;
            vkContext->builtInSceneBackgroundShader = shader;
            NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.BackgroundShader assigned to sceneRenderpass.");

            auto* gameBackgroundShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
            gameBackgroundShader->stagesData = shader->stagesData;
            gameBackgroundShader->reflection = shader->reflection;

            if (!NOUS_VulkanShader::Create(vkContext, gameRenderpassTarget, gameBackgroundShader,
                                            {.noDepthTest = true}))
            {
                NOUS_WARN_C(CURRENT_CHANNEL, "[CreateShader] Failed to create game-renderpass background variant.");
                NOUS_DELETE(gameBackgroundShader, MemoryTag::RESOURCE_SHADER);
            }
            else
            {
                vkContext->builtInGameBackgroundShader = gameBackgroundShader;
                NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.BackgroundShader clone assigned to gameRenderpass.");
            }
        }

        return true;
    }

    // ── BuiltIn.BoundingBoxShader → scene renderpass (editor bounding box overlay) ─
    //    Uses LINE_LIST topology. Scene viewport only; no game renderpass clone needed.
    if (assetPath.find("BuiltIn.BoundingBoxShader") != std::string::npos)
    {
        if (!NOUS_VulkanShader::Create(vkContext, &vkContext->sceneRenderpass, shader,
                                        {.useLineTopology = true}))
            return false;
        vkContext->builtInBoundingBoxShader = shader;
        NOUS_INFO_C(CURRENT_CHANNEL, "[CreateShader] BuiltIn.BoundingBoxShader assigned to sceneRenderpass (LINE_LIST).");
        return true;
    }

    // ── Default: user-defined shaders — target the active renderpass ─────────
    //    In GAME mode sceneRenderpass is VK_NULL_HANDLE; use the swapchain renderpass instead.
    VulkanRenderpass* targetRenderpass = vkContext->renderMode == RenderMode::GAME
        ? &vkContext->gameSwapchainRenderpass
        : &vkContext->sceneRenderpass;
    if (!NOUS_VulkanShader::Create(vkContext, targetRenderpass, shader))
        return false;
    TryWriteInstanceSSBODescriptor(vkContext, shader);
    return true;
}

void VulkanBackend::DestroyShader(ResourceShader* shader) noexcept
{
    if (!shader || !shader->internalData)
        return;

    // Null out vkContext built-in pointer so Shutdown() doesn't touch freed memory.
    if (shader == vkContext->builtInMaterialShader)         vkContext->builtInMaterialShader         = nullptr;
    if (shader == vkContext->builtInOutlineShader)          vkContext->builtInOutlineShader           = nullptr;
    if (shader == vkContext->builtInGridShader)             vkContext->builtInGridShader              = nullptr;
    if (shader == vkContext->builtInSceneBackgroundShader)  vkContext->builtInSceneBackgroundShader   = nullptr;
    if (shader == vkContext->builtInBoundingBoxShader)      vkContext->builtInBoundingBoxShader       = nullptr;
    // In GAME mode these are ResourceManager-owned and point directly to the shader being destroyed.
    if (shader == vkContext->builtInGameShader)             vkContext->builtInGameShader             = nullptr;
    if (shader == vkContext->builtInGameBackgroundShader)   vkContext->builtInGameBackgroundShader   = nullptr;

    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    auto* vs = down_cast<VulkanShader*>(shader->internalData);
    NOUS_VulkanShader::Destroy(vkContext, vs);
    shader->internalData = nullptr;
}

bool VulkanBackend::ReloadShader(ResourceShader* shader) noexcept
{
    if (!shader)
        return false;

    const std::string& assetPath = shader->GetAssetsPath();
    NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Reloading '%s'...", assetPath.c_str());

    // ── 1. Recompile from source (CPU only — no GPU work yet) ─────────────────
    const ShaderCompilerConfig config;
    ShaderLoadResult loadResult = nous::engine::shader_system::LoadShaderFromFile(assetPath, config);

    if (!loadResult.success)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Compile failed for '%s': %s",
                     assetPath.c_str(), loadResult.errorMessage.c_str());
        if (loadResult.shader)
            NOUS_DELETE(loadResult.shader, MemoryTag::RESOURCE_SHADER);
        return false;   // old GPU pipeline left intact
    }

    // ── 2. Swap CPU data on the primary ResourceShader ────────────────────────
    shader->stagesData = std::move(loadResult.shader->stagesData);
    shader->reflection = std::move(loadResult.shader->reflection);
    shader->generation++;
    NOUS_DELETE(loadResult.shader, MemoryTag::RESOURCE_SHADER);

    // ── 3. GPU swap ───────────────────────────────────────────────────────────
    return ApplyCompiledShader(shader);
}

bool VulkanBackend::ApplyCompiledShader(ResourceShader* shader) noexcept
{
    if (!shader)
        return false;

    const std::string& assetPath = shader->GetAssetsPath();

    // ── GPU drain ─────────────────────────────────────────────────────────────
    // Called once here; covers all destroy/create work below.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // Helper: destroys existing GPU data on `s` (if any) and recreates it.
    // GPU is already idle — vkDeviceWaitIdle was called above.
    auto recreate = [&](ResourceShader* s, VulkanRenderpass* rp,
                        const VulkanShaderCreateInfo& settings = {}) -> bool
    {
        if (s->internalData)
        {
            auto* oldVS = down_cast<VulkanShader*>(s->internalData);
            NOUS_VulkanShader::Destroy(vkContext, oldVS);
            s->internalData = nullptr;
        }
        return NOUS_VulkanShader::Create(vkContext, rp, s, settings);
    };

    // Helper: re-acquires descriptor-set instance slots for every loaded material.
    // Must be called after the MaterialShader pools are destroyed+recreated — old
    // descriptor sets are gone with the old pool, so each material needs a fresh slot.
    auto reacquireMaterialInstances = [&]
    {
        if (!vkContext->resourceManager) return;
        vkContext->resourceManager->ForEachMaterial([this](ResourceMaterial* mat)
        {
            mat->internalID      = INVALID_ID;  // bypass the "already acquired" guard
            mat->poolOwnerShader = nullptr;      // let CreateMaterial re-derive the pool owner
            CreateMaterial(mat);
        });
    };

    // ── Recreate GPU resources (mirrors CreateShader path selection) ──────────

    // ── BuiltIn.MaterialShader ────────────────────────────────────────────────
    if (assetPath.find("BuiltIn.MaterialShader") != std::string::npos)
    {
        if (vkContext->renderMode == RenderMode::GAME)
        {
            if (!recreate(shader, &vkContext->gameSwapchainRenderpass))
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate MaterialShader (GAME mode).");
                return false;
            }
            TryWriteInstanceSSBODescriptor(vkContext, shader);
            reacquireMaterialInstances();
            NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] MaterialShader reloaded (GAME mode, gen=%u).", shader->generation);
            return true;
        }

        // EDITOR mode: primary on sceneRenderpass.
        if (!recreate(shader, &vkContext->sceneRenderpass))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate MaterialShader (scene).");
            return false;
        }
        TryWriteInstanceSSBODescriptor(vkContext, shader);

        // Game clone on gameRenderpass.
        if (vkContext->builtInGameShader)
        {
            vkContext->builtInGameShader->stagesData = shader->stagesData;
            vkContext->builtInGameShader->reflection = shader->reflection;
            vkContext->builtInGameShader->generation = shader->generation;
            if (!recreate(vkContext->builtInGameShader, &vkContext->gameRenderpass))
                NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate MaterialShader game clone.");
            else
                TryWriteInstanceSSBODescriptor(vkContext, vkContext->builtInGameShader);
        }

        // NOTE: builtInPickShader is NOT updated here. It is a separate independent shader
        // (BuiltIn.PickShader.glsl) that outputs object IDs for mouse picking — it is NOT
        // a clone of the material shader and must not be overwritten with material shader data.

        // Re-acquire descriptor-set slots for all materials — old slots were destroyed
        // with the previous shader pool and must be reallocated from the new one.
        reacquireMaterialInstances();

        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] MaterialShader reloaded (EDITOR mode, gen=%u).", shader->generation);
        return true;
    }

    // ── BuiltIn.PickShader ────────────────────────────────────────────────────
    //    The ResourceManager holds CPU metadata; GPU lives in builtInPickShader clone.
    if (assetPath.find("BuiltIn.PickShader") != std::string::npos)
    {
        if (vkContext->builtInPickShader)
        {
            vkContext->builtInPickShader->stagesData = shader->stagesData;
            vkContext->builtInPickShader->reflection = shader->reflection;
            vkContext->builtInPickShader->generation = shader->generation;
            if (!recreate(vkContext->builtInPickShader, &vkContext->pickRenderpass, {.disableBlending = true}))
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate PickShader.");
                return false;
            }
        }
        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] PickShader reloaded.");
        return true;
    }

    // ── BuiltIn.OutlineShader ─────────────────────────────────────────────────
    if (assetPath.find("BuiltIn.OutlineShader") != std::string::npos)
    {
        if (!recreate(shader, &vkContext->sceneRenderpass, {.createOutlinePipelines = true}))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate OutlineShader.");
            return false;
        }
        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] OutlineShader reloaded.");
        return true;
    }

    // ── BuiltIn.GridShader ────────────────────────────────────────────────────
    if (assetPath.find("BuiltIn.GridShader") != std::string::npos)
    {
        if (!recreate(shader, &vkContext->sceneRenderpass, {.useLineTopology = true}))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate GridShader.");
            return false;
        }
        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] GridShader reloaded.");
        return true;
    }

    // ── BuiltIn.BackgroundShader ──────────────────────────────────────────────
    if (assetPath.find("BuiltIn.BackgroundShader") != std::string::npos)
    {
        if (vkContext->renderMode == RenderMode::GAME)
        {
            if (!recreate(shader, &vkContext->gameSwapchainRenderpass, {.noDepthTest = true}))
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate BackgroundShader (GAME mode).");
                return false;
            }
            NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] BackgroundShader reloaded (GAME mode).");
            return true;
        }

        // EDITOR mode: primary on sceneRenderpass.
        if (!recreate(shader, &vkContext->sceneRenderpass, {.noDepthTest = true}))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate BackgroundShader (scene).");
            return false;
        }

        // Game background clone on gameRenderpass.
        if (vkContext->builtInGameBackgroundShader)
        {
            vkContext->builtInGameBackgroundShader->stagesData = shader->stagesData;
            vkContext->builtInGameBackgroundShader->reflection = shader->reflection;
            vkContext->builtInGameBackgroundShader->generation = shader->generation;
            if (!recreate(vkContext->builtInGameBackgroundShader, &vkContext->gameRenderpass, {.noDepthTest = true}))
                NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate BackgroundShader game clone.");
        }

        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] BackgroundShader reloaded (EDITOR mode).");
        return true;
    }

    // ── BuiltIn.BoundingBoxShader ─────────────────────────────────────────────
    if (assetPath.find("BuiltIn.BoundingBoxShader") != std::string::npos)
    {
        if (!recreate(shader, &vkContext->sceneRenderpass, {.useLineTopology = true}))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate BoundingBoxShader.");
            return false;
        }
        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] BoundingBoxShader reloaded.");
        return true;
    }

    // ── Default: user-defined shader ─────────────────────────────────────────
    VulkanRenderpass* reloadTargetRenderpass = vkContext->renderMode == RenderMode::GAME
        ? &vkContext->gameSwapchainRenderpass
        : &vkContext->sceneRenderpass;
    if (!recreate(shader, reloadTargetRenderpass))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Failed to recreate shader '%s'.", assetPath.c_str());
        return false;
    }
    TryWriteInstanceSSBODescriptor(vkContext, shader);

    // Custom shaders own their own instance pool when used as poolOwnerShader for a
    // material (see CreateMaterial: it picks the custom shader's pool if it is
    // GPU_READY). Recreating the VulkanShader above destroyed that pool, so every
    // material whose poolOwnerShader pointed at this shader now holds a stale
    // internalID slot referencing a destroyed VkDescriptorSet. Drawing those
    // materials triggers VUID-vkCmdBindDescriptorSets-graphicsPipelineLibrary-06754
    // ("pDescriptorSets[0] (VkDescriptorSet 0x0[])"). Re-acquire slots from the
    // freshly created pool — the same pattern used for MaterialShader reloads.
    reacquireMaterialInstances();

    NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Shader '%s' reloaded (gen=%u).", assetPath.c_str(), shader->generation);
    return true;
}

uint32 VulkanBackend::PickObjectAt(int32 pixelX, int32 pixelY,
                                   const glm::mat4& projection, const glm::mat4& view,
                                   const std::vector<GeometryRenderData>& geometries)
{
    ResourceShader* rPickShader = vkContext->builtInPickShader;
    if (!rPickShader || !rPickShader->internalData)
    {
        NOUS_WARN_C(LogChannel::NOUS_ENGINE_EDITOR_FEATURE_MOUSE_PICKING, "Pick shader not available.");
        return 0;
    }

    // Clamp pixel coordinates to framebuffer bounds.
    if (pixelX < 0 || pixelX >= vkContext->framebufferWidth ||
        pixelY < 0 || pixelY >= vkContext->framebufferHeight)
    {
        return 0;
    }

    auto pickVS = down_cast<VulkanShader*>(rPickShader->internalData);

    // Wait for all GPU work to complete before using the pick resources.
    vkDeviceWaitIdle(vkContext->device.logicalDevice);

    // --- Allocate single-use command buffer ---
    VulkanCommandBuffer cmdBuffer{};
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
    if (void* mapped = NOUS_VulkanBuffer::LockMemory(vkContext, &stagingBuffer, 0, 4, 0))
    {
        const auto* pixel = static_cast<const uint8*>(mapped);
        objectID = static_cast<uint32>(pixel[0])
                 | static_cast<uint32>(pixel[1]) << 8
                 | static_cast<uint32>(pixel[2]) << 16
                 | static_cast<uint32>(pixel[3]) << 24;
        NOUS_VulkanBuffer::UnlockMemory(vkContext, &stagingBuffer);
    }

    // --- Cleanup staging buffer ---
    NOUS_VulkanBuffer::DestroyBuffer(vkContext, &stagingBuffer);

    NOUS_DEBUG_C(LogChannel::NOUS_ENGINE_EDITOR_FEATURE_MOUSE_PICKING,
        "Pixel (%d, %d) -> objectUID = %u", pixelX, pixelY, objectID);

    return objectID;
}

// ─────────────────────────────────────────────────────────────────────────────
// IEditorRenderBridge — the editor-facing view of this backend.
// See Renderer/iEditorRenderBridge.h for why this exists.
// ─────────────────────────────────────────────────────────────────────────────

EditorGpuInfo VulkanBackend::GetGpuInfo() const
{
    EditorGpuInfo info{};
    if (!vkContext)
        return info;

    info.instance            = vkContext->instance;
    info.physicalDevice      = vkContext->device.physicalDevice;
    info.device              = vkContext->device.logicalDevice;
    info.graphicsQueueFamily = vkContext->device.graphicsQueueIndex;
    info.graphicsQueue       = vkContext->device.graphicsQueue;
    info.descriptorPool      = vkContext->imGuiResources.descriptorPool;
    info.uiRenderpass        = vkContext->uiRenderpass.handle;
    info.allocator           = vkContext->allocator;
    info.imageCount          = static_cast<uint32>(vkContext->swapChain.swapChainImages.size());

    // Hand over this TU's copy, so the reporting goes through the engine-side
    // VkResultMessage and the editor needs no renderer-internal Vulkan header.
    info.checkVkResultFn     = VK_CHECK_IMGUI;

    return info;
}

void VulkanBackend::DestroyImGuiResources()
{
    if (vkContext)
        NOUS_ImGuiVulkanResources::DestroyImGuiVulkanResources(vkContext);
}

void VulkanBackend::RecreateImGuiResources()
{
    if (vkContext)
        NOUS_ImGuiVulkanResources::RecreateImGuiVulkanResources(vkContext);
}

VkCommandBuffer VulkanBackend::GetCurrentUICommandBuffer() const
{
    if (!vkContext || vkContext->imageIndex >= vkContext->graphicsCommandBuffers.size())
        return VK_NULL_HANDLE;

    return vkContext->graphicsCommandBuffers[vkContext->imageIndex].handle;
}

uint64 VulkanBackend::GetViewportTexture(const EditorViewport viewport) const
{
    if (!vkContext)
        return 0;

    const auto& descriptorSets = (viewport == EditorViewport::Scene)
        ? vkContext->imGuiResources.m_ViewportDescriptorSets
        : vkContext->imGuiResources.m_GameViewportDescriptorSets;

    if (vkContext->imageIndex >= descriptorSets.size())
        return 0;

    return NOUS_ImGuiVulkanResources::GetViewportTexture(descriptorSets[vkContext->imageIndex]);
}

EditorViewportImages VulkanBackend::GetViewportImages(const EditorViewport viewport) const
{
    EditorViewportImages out{};
    if (!vkContext)
        return out;

    const bool isScene = (viewport == EditorViewport::Scene);

    const auto& images = isScene ? vkContext->imGuiResources.m_ViewportImages
                                 : vkContext->imGuiResources.m_GameViewportImages;

    out.sampler = isScene ? vkContext->imGuiResources.m_ViewportTextureSampler
                          : vkContext->imGuiResources.m_GameViewportTextureSampler;

    out.views.reserve(images.size());
    for (const auto& image : images)
        out.views.push_back(image.view);

    return out;
}

void VulkanBackend::SetViewportDescriptorSets(const EditorViewport viewport,
                                              std::vector<VkDescriptorSet> descriptorSets)
{
    if (!vkContext)
        return;

    auto& target = (viewport == EditorViewport::Scene)
        ? vkContext->imGuiResources.m_ViewportDescriptorSets
        : vkContext->imGuiResources.m_GameViewportDescriptorSets;

    target = std::move(descriptorSets);
}

std::vector<VkDescriptorSet> VulkanBackend::TakeViewportDescriptorSets(const EditorViewport viewport)
{
    if (!vkContext)
        return {};

    auto& source = (viewport == EditorViewport::Scene)
        ? vkContext->imGuiResources.m_ViewportDescriptorSets
        : vkContext->imGuiResources.m_GameViewportDescriptorSets;

    // Move-and-clear: the caller must ImGui_ImplVulkan_RemoveTexture exactly the
    // sets that exist, which can be fewer than the image count before Create ran.
    std::vector<VkDescriptorSet> taken = std::move(source);
    source.clear();

    return taken;
}

void VulkanBackend::GetFramebufferSize(int32* outWidth, int32* outHeight) const
{
    if (outWidth)  *outWidth  = vkContext ? vkContext->framebufferWidth  : 0;
    if (outHeight) *outHeight = vkContext ? vkContext->framebufferHeight : 0;
}

void VulkanBackend::RecreateWorkerCommandPools()
{
    if (vkContext)
        NOUS_VulkanMultithreading::RecreateWorkerCommandPools(vkContext);
}

bool VulkanBackend::DrawGrid(const RenderpassType renderpassID,
                             const glm::mat4& projection, const glm::mat4& view)
{
#ifdef _PROFILING
    ZoneScopedN("DrawGrid");
#endif
    // Grid is scene-viewport only.
    if (renderpassID != RenderpassType::SCENE)
        return true;

    const ResourceShader* rGridShader = vkContext->builtInGridShader;
    if (!rGridShader || !rGridShader->internalData)
        return true; // Grid shader not loaded yet — skip gracefully.

    if (vkContext->gridVertexBuffer.handle == VK_NULL_HANDLE || vkContext->gridVertexCount == 0)
        return true;

    const VulkanCommandBuffer* cmdBuf = GetCommandBufferByRenderpassID(renderpassID);
    const auto vs = down_cast<VulkanShader*>(rGridShader->internalData);

    // Bind the grid pipeline and upload view/projection to the grid shader's global UBO.
    NOUS_VulkanShader::BindPipeline(cmdBuf->handle, vs);

    const struct GlobalUBO { glm::mat4 projection; glm::mat4 view; } ubo{ projection, view };
    NOUS_VulkanShader::UpdateGlobal(vkContext, cmdBuf->handle, vs,
        vkContext->imageIndex, &ubo, sizeof(ubo));

    // Push identity model matrix (grid lives at the world origin).
    constexpr glm::mat4 identity(1.0f);
    vkCmdPushConstants(cmdBuf->handle, vs->pipeline.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &identity);

    // Bind the grid vertex buffer (axis lines first, minor lines after).
    constexpr VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdBuf->handle, 0, 1, &vkContext->gridVertexBuffer.handle, &offset);

    const bool supportsWideLines = vkContext->device.features.wideLines == VK_TRUE;

    // ── Axis lines (first 4 vertices: X axis + Z axis) ────────────────────────
    vkCmdSetLineWidth(cmdBuf->handle, supportsWideLines ? 3.0f : 1.0f);
    vkCmdDraw(cmdBuf->handle, 4, 1, 0, 0);

    // ── Minor grid lines (remaining vertices) ─────────────────────────────────
    vkCmdSetLineWidth(cmdBuf->handle, 1.0f);
    vkCmdDraw(cmdBuf->handle, vkContext->gridVertexCount - 4, 1, 4, 0);

    return true;
}

bool VulkanBackend::DrawBackground(const RenderpassType renderpassID,
                                    const glm::mat4& projection,
                                    const glm::mat4& view)
{
#ifdef _PROFILING
    ZoneScopedN("DrawBackground");
#endif
    // Select the shader for this renderpass.
    const ResourceShader* rShader = nullptr;
    if (renderpassID == RenderpassType::SCENE)
        rShader = vkContext->builtInSceneBackgroundShader;
    else if (renderpassID == RenderpassType::GAME)
        rShader = vkContext->builtInGameBackgroundShader;

    if (!rShader || !rShader->internalData)
        return true; // Shader not loaded yet — skip gracefully.

    const VulkanCommandBuffer* cmdBuf = GetCommandBufferByRenderpassID(renderpassID);
    const auto vs = down_cast<VulkanShader*>(rShader->internalData);

    // Bind the background pipeline (no vertex buffers, no descriptor sets needed).
    NOUS_VulkanShader::BindPipeline(cmdBuf->handle, vs);

    // ── Compute the UV-y of the world y=0 horizon line ────────────────────────
    //
    // For a perspective camera, the world-space Y of a ray through NDC (x, y) is:
    //   worldDir.y = right.y * x/P00  +  up.y * y/P11  +  forward.y
    //
    // Setting worldDir.y = 0 (horizon), and assuming no camera roll (right.y = 0):
    //   horizonNDC_y = -forward.y * P11 / up.y
    //
    // GLM mat4 is column-major: mat[col][row].
    // View matrix rows: [right | up | -forward | translation]
    //   view[1][1] = up.y       (column 1, row 1)
    //   view[1][2] = -forward.y (column 1, row 2)  →  forward.y = -view[1][2]
    //   proj[1][1] = 1/tan(fovY/2)
    //
    // Then convert NDC to UV:  UV.y = (NDC.y + 1) / 2
    // (with the inverted Vulkan viewport: NDC.y=-1 is screen bottom → UV.y=0,
    //                                     NDC.y=+1 is screen top   → UV.y=1)
    const float upY     = view[1][1];   // world Y projected onto camera up
    const float negFwdY = view[1][2];   // -forward.y
    const float focalY  = projection[1][1];

    float horizonNDC_y;
    if (std::abs(upY) > 1e-4f)
        horizonNDC_y = negFwdY * focalY / upY;  // = -forward.y * P11 / up.y
    else
        horizonNDC_y = negFwdY >= 0.0f ? 100.0f : -100.0f; // camera near-vertical: off screen

    // Allow a small margin outside [0,1] so the gradient still looks natural
    // when the horizon is just off the edge of the screen.
    const float horizonY = std::clamp((horizonNDC_y + 1.0f) * 0.5f, -0.5f, 1.5f);

    // ── Push constants ─────────────────────────────────────────────────────────
    struct BackgroundPushConstants
    {
        glm::vec4 skyColor;     // offset  0 — deep blue at top and bottom edges
        glm::vec4 horizonColor; // offset 16 — near-white at the world y=0 line
        float     horizonY;     // offset 32 — UV-y of the horizon (computed above)
    };
    const BackgroundPushConstants push
    {
        glm::vec4(0.08f, 0.14f, 0.32f, 1.0f), // sky: deep blue
        glm::vec4(0.92f, 0.95f, 1.00f, 1.0f), // horizon: near-white
        horizonY
    };

    // Size = offset of last field + its size = 32 + 4 = 36 bytes.
    constexpr uint32_t k_PushSize =
        offsetof(BackgroundPushConstants, horizonY) + sizeof(float);

    vkCmdPushConstants(cmdBuf->handle, vs->pipeline.pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, k_PushSize, &push);

    // Draw the fullscreen triangle — no vertex buffer needed.
    vkCmdDraw(cmdBuf->handle, 3, 1, 0, 0);

    return true;
}

bool VulkanBackend::DrawWireframeMeshInstances(const RenderpassType renderpassID,
                                               const glm::mat4& projection,
                                               const glm::mat4& view,
                                               const WireframeMesh mesh,
                                               const std::vector<WireframeInstance>& instances)
{
#ifdef _PROFILING
    ZoneScopedN("DrawWireframeMeshInstances");
#endif
    // Wireframe debug meshes are scene-viewport only.
    if (renderpassID != RenderpassType::SCENE)
        return true;

    if (instances.empty())
        return true;

    // All wireframe meshes reuse the bounding-box shader: same vertex format
    // (Vertex3D.position), same GlobalUBO (projection + view), same push
    // constants (mat4 model + vec4 color).
    const ResourceShader* rShader = vkContext->builtInBoundingBoxShader;
    if (!rShader || !rShader->internalData)
        return true; // Shader not loaded yet — skip gracefully.

    // Map the mesh kind to its shared static vertex buffer + line width.
    const VulkanBuffer* vertexBuffer = nullptr;
    uint32 vertexCount = 0;
    float  lineWidth   = 1.5f;
    switch (mesh)
    {
        case WireframeMesh::Cube:
            vertexBuffer = &vkContext->boundingBoxVertexBuffer;
            vertexCount  = vkContext->boundingBoxVertexCount;
            lineWidth    = 2.0f; // bounding boxes drawn slightly thicker
            break;
        case WireframeMesh::Sphere:
            vertexBuffer = &vkContext->pointLightSphereVertexBuffer;
            vertexCount  = vkContext->pointLightSphereVertexCount;
            break;
        case WireframeMesh::Pyramid:
            vertexBuffer = &vkContext->dirLightPyramidVertexBuffer;
            vertexCount  = vkContext->dirLightPyramidVertexCount;
            break;
        case WireframeMesh::Cone:
            vertexBuffer = &vkContext->spotLightConeVertexBuffer;
            vertexCount  = vkContext->spotLightConeVertexCount;
            break;
        default:
            return true;
    }

    if (vertexBuffer->handle == VK_NULL_HANDLE || vertexCount == 0)
        return true;

    const VulkanCommandBuffer* cmdBuf = GetCommandBufferByRenderpassID(renderpassID);
    const auto vs = down_cast<VulkanShader*>(rShader->internalData);

    NOUS_VulkanShader::BindPipeline(cmdBuf->handle, vs);

    if (vkContext->wireframeGlobalSetThisFrame)
    {
        // A previous wireframe draw already updated set=0 this frame. Updating it
        // again while bound would invalidate the command buffer — just rebind it
        // (same projection/view). See the set=0 inheritance note in iRendererBackend.h.
        vkCmdBindDescriptorSets(cmdBuf->handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vs->pipeline.pipelineLayout, 0, 1,
            &vs->globalDescriptorSets[vkContext->imageIndex], 0, nullptr);
    }
    else
    {
        const struct GlobalUBO { glm::mat4 projection; glm::mat4 view; } ubo{ projection, view };
        NOUS_VulkanShader::UpdateGlobal(vkContext, cmdBuf->handle, vs,
            vkContext->imageIndex, &ubo, sizeof(ubo));
        vkContext->wireframeGlobalSetThisFrame = true;
    }

    // Bind the shared per-mesh wireframe vertex buffer once.
    constexpr VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdBuf->handle, 0, 1, &vertexBuffer->handle, &offset);

    const bool supportsWideLines = vkContext->device.features.wideLines == VK_TRUE;
    vkCmdSetLineWidth(cmdBuf->handle, supportsWideLines ? lineWidth : 1.0f);

    // Push constant layout: mat4 model (64 bytes) + vec4 color (16 bytes) = 80 bytes.
    struct WireframePushConstants { glm::mat4 model; glm::vec4 color; };

    for (const WireframeInstance& inst : instances)
    {
        WireframePushConstants pc{ inst.transform, inst.color };
        vkCmdPushConstants(cmdBuf->handle, vs->pipeline.pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(WireframePushConstants), &pc);

        vkCmdDraw(cmdBuf->handle, vertexCount, 1, 0, 0);
    }

    return true;
}

bool VulkanBackend::DrawCameraFrustums(RenderpassType renderpassID,
                                        const glm::mat4& projection,
                                        const glm::mat4& view,
                                        const std::vector<CameraFrustumData>& frustums)
{
#ifdef _PROFILING
    ZoneScopedN("DrawCameraFrustums");
#endif
    // Frustum visualization is scene-viewport only.
    if (renderpassID != RenderpassType::SCENE)
        return true;

    if (frustums.empty())
        return true;

    // Reuse the bounding box shader: same vertex format (Vertex3D.position),
    // same GlobalUBO (projection + view), same push constants (model + color).
    ResourceShader* rShader = vkContext->builtInBoundingBoxShader;
    if (!rShader || !rShader->internalData)
        return true;

    if (vkContext->frustumVertexBuffer.handle == VK_NULL_HANDLE || vkContext->frustumVertexCapacity == 0)
        return true;

    // Corner winding order: [0]=nearTL [1]=nearTR [2]=nearBR [3]=nearBL
    //                        [4]=farTL  [5]=farTR  [6]=farBR  [7]=farBL
    // 12 edges as LINE_LIST (24 vertex indices):
    //   near quad:       0-1, 1-2, 2-3, 3-0
    //   far quad:        4-5, 5-6, 6-7, 7-4
    //   connecting:      0-4, 1-5, 2-6, 3-7
    constexpr int k_EdgeIndices[24] = {
        0,1, 1,2, 2,3, 3,0,   // near quad
        4,5, 5,6, 6,7, 7,4,   // far quad
        0,4, 1,5, 2,6, 3,7    // connecting edges
    };

    // Build interleaved world-space vertex list for all frustums.
    std::vector<Vertex3D> verts;
    verts.reserve(frustums.size() * 24);

    for (const CameraFrustumData& f : frustums)
    {
        for (int k_EdgeIndice : k_EdgeIndices)
        {
            Vertex3D v{};
            v.position = f.corners[k_EdgeIndice];
            verts.push_back(v);
        }
    }

    const auto totalVerts = static_cast<uint32>(verts.size());
    if (totalVerts > vkContext->frustumVertexCapacity)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[DrawCameraFrustums] Frustum vertex count (%u) exceeds buffer capacity (%u). Skipping.",
            totalVerts, vkContext->frustumVertexCapacity);
        return true;
    }

    NOUS_VulkanBuffer::LoadData(vkContext, &vkContext->frustumVertexBuffer,
        0, totalVerts * sizeof(Vertex3D), 0, verts.data());

    VulkanCommandBuffer* cmdBuf = GetCommandBufferByRenderpassID(renderpassID);
    auto vs = down_cast<VulkanShader*>(rShader->internalData);

    NOUS_VulkanShader::BindPipeline(cmdBuf->handle, vs);

    if (vkContext->wireframeGlobalSetThisFrame)
    {
        // A prior wireframe draw (DrawWireframeMeshInstances) already called
        // UpdateGlobal this frame, which bound the global descriptor set (set=0)
        // into this command buffer. Calling vkUpdateDescriptorSets on it again
        // would invalidate the CB. Just rebind it — no update needed (same
        // projection/view matrices).
        vkCmdBindDescriptorSets(cmdBuf->handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vs->pipeline.pipelineLayout, 0, 1,
            &vs->globalDescriptorSets[vkContext->imageIndex], 0, nullptr);
    }
    else
    {
        // First (or only) use of this shader this frame — full update.
        struct GlobalUBO { glm::mat4 projection; glm::mat4 view; } ubo{ projection, view };
        NOUS_VulkanShader::UpdateGlobal(vkContext, cmdBuf->handle, vs,
            vkContext->imageIndex, &ubo, sizeof(ubo));
        vkContext->wireframeGlobalSetThisFrame = true;
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdBuf->handle, 0, 1, &vkContext->frustumVertexBuffer.handle, &offset);

    const bool supportsWideLines = vkContext->device.features.wideLines == VK_TRUE;
    vkCmdSetLineWidth(cmdBuf->handle, supportsWideLines ? 1.5f : 1.0f);

    // model = identity: frustum vertices are already in world space.
    struct FrustumPushConstants { glm::mat4 model; glm::vec4 color; };

    for (uint32 i = 0; i < static_cast<uint32>(frustums.size()); ++i)
    {
        FrustumPushConstants pc{ glm::mat4(1.0f), frustums[i].color };
        vkCmdPushConstants(cmdBuf->handle, vs->pipeline.pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FrustumPushConstants), &pc);

        vkCmdDraw(cmdBuf->handle, 24, 1, i * 24, 0);
    }

    return true;
}

bool VulkanBackend::DrawOutlinedGeometries(const RenderpassType renderpassID,
                                            const glm::mat4& projection, const glm::mat4& view,
                                            const std::vector<GeometryRenderData>& outlinedGeometries,
                                            const OutlineSettings& settings)
{
#ifdef _PROFILING
    ZoneScopedN("DrawOutlinedGeometries");
#endif
    // Outline effect is scene-viewport only.
    if (renderpassID != RenderpassType::SCENE)
        return true;

    if (outlinedGeometries.empty())
        return true;

    const ResourceShader* rOutlineShader = vkContext->builtInOutlineShader;
    if (!rOutlineShader || !rOutlineShader->internalData)
        return true; // Outline shader not loaded yet — skip gracefully.

    const VulkanCommandBuffer* commandBuffer = GetCommandBufferByRenderpassID(renderpassID);
    const auto vs = down_cast<VulkanShader*>(rOutlineShader->internalData);

    // Upload the outline global UBO: projection + view + outlineColor.
    const struct OutlineGlobalUBO { glm::mat4 projection; glm::mat4 view; glm::vec4 outlineColor; }
        globalUBO{ projection, view, settings.color };

    // ── Pass 1: Stencil-write ─────────────────────────────────────────────────
    // Draw the selected mesh at its original scale. The stencil-write pipeline writes
    // stencil=1 for every visible pixel (depth test ON, colour write OFF).

    if (settings.depthAware)
        NOUS_VulkanShader::BindStencilWritePipeline(commandBuffer->handle, vs);
    else
        NOUS_VulkanShader::BindStencilWriteNoDepthPipeline(commandBuffer->handle, vs);

    NOUS_VulkanShader::UpdateGlobal(vkContext, commandBuffer->handle, vs,
        vkContext->imageIndex, &globalUBO, sizeof(globalUBO));

    for (const auto& renderData : outlinedGeometries)
    {
        if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID) continue;

        const VulkanGeometryData* bufferData = &vkContext->geometries[renderData.geometry->internalID];

        // Pass 1: draw the mesh at its ORIGINAL position (thickness=0) to mark the
        // stencil buffer at the real silhouette — NOT the expanded shell.
        struct OutlinePushConstant
        {
            glm::mat4 model;
            float thickness;
        } pc{.model = renderData.model, .thickness = 0.0f};

        vkCmdPushConstants(
    commandBuffer->handle,
    vs->pipeline.pipelineLayout,
    VK_SHADER_STAGE_VERTEX_BIT,
    0,
    sizeof(OutlinePushConstant),
    &pc);

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
    // Draw each mesh again with normal extrusion (thickness = settings.width).
    // The outline-draw pipeline uses stencil NOTEQUAL(1), so only the border ring
    // between the original silhouette and the expanded shell is coloured.

    if (settings.depthAware)
        NOUS_VulkanShader::BindPipeline(commandBuffer->handle, vs);
    else
        NOUS_VulkanShader::BindOutlineNoDepthPipeline(commandBuffer->handle, vs);

    for (const auto& renderData : outlinedGeometries)
    {
        if (!renderData.geometry || renderData.geometry->internalID == INVALID_ID) continue;

        const VulkanGeometryData* bufferData = &vkContext->geometries[renderData.geometry->internalID];

        struct OutlinePushConstant
        {
            glm::mat4 model;
            float thickness;
        } pc{.model = renderData.model, .thickness = settings.width};

        vkCmdPushConstants(
    commandBuffer->handle,
    vs->pipeline.pipelineLayout,
    VK_SHADER_STAGE_VERTEX_BIT,
    0,
    sizeof(OutlinePushConstant),
    &pc);

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
#ifdef _PROFILING
    ZoneScopedN("ProcessPendingSubmissions");
#endif
    std::unique_lock lock(vkContext->submitQueueMutex);

    while (!vkContext->submitQueue.empty()) 
    {
        auto task = std::move(vkContext->submitQueue.front());
        vkContext->submitQueue.pop_front();

        lock.unlock(); // Unlock while processing

        std::mutex& queueMutex = task.queue == vkContext->device.transferQueue
            ? vkContext->device.transferQueueMutex
            : vkContext->device.graphicsQueueMutex;
        std::lock_guard queueLock(queueMutex);

        bool success;
        {
#ifdef _PROFILING
            ZoneScopedN("vkQueueSubmit");
#endif
            success = vkQueueSubmit(task.queue, task.submitCount, task.pSubmits, task.fence) == VK_SUCCESS;
        }
        if (success && task.waitIdle)
        {
#ifdef _PROFILING
            ZoneScopedN("vkQueueWaitIdle");
#endif
            success = vkQueueWaitIdle(task.queue) == VK_SUCCESS;
        }

        task.resultPromise.set_value(success);

        lock.lock(); // Re-lock for next iteration
    }
}
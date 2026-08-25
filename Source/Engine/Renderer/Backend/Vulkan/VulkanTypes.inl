#ifndef VULKANTYPES_INL
#define VULKANTYPES_INL

#include "Engine/Core/Globals.h"
#include "Engine/Renderer/RendererTypes.h"

#include <vulkan/vulkan.h>

#include <future>
#include <thread>
#include <array>
#include <deque>
#include <unordered_map>
#include <vector>

class Freelist;

// Forward declarations for injected dependencies
class IRenderWindow;
class IRenderResourceProvider;
class EventSystem;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

struct VulkanImage
{
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;

    uint32 width;
    uint32 height;
};

enum class VulkanRenderPassState : uint8_t
{
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED
};

struct VulkanRenderpass 
{
    VkRenderPass handle;

    glm::vec4 renderArea;
    glm::vec4 clearColor;

    float depth;
    uint32 stencil;

    uint8 clearFlags;
    bool prevPass;
    bool nextPass;

    VulkanRenderPassState state;
};

struct VulkanBuffer 
{
    VkBuffer handle;
    VkBufferUsageFlagBits usage;

    VkDeviceMemory memory;
    int32 memoryIndex;
    uint32 memoryPropertyFlags;

    uint64 totalSize;
    bool isLocked;

    // ------- Freelist ------- //
    uint64 freelistMemoryRequirement;   // The amount of memory required for the freelist.
    void* freelistBlock;                // The memory block used by the internal freelist.
    Freelist* bufferFreelist;           // A freelist to track allocations.
};

struct VulkanSwapChain
{
    VkSwapchainKHR handle;
    
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    uint8 maxFramesInFlight;

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VulkanImage colorAttachment;
    VulkanImage depthAttachment;

    // One framebuffer per swapchain image; sized to swapChainImages.size() in CreateFramebuffers.
    std::vector<VkFramebuffer> swapChainFramebuffers;
};

struct VkSwapChainSupportDetails
{
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
    VkSurfaceCapabilitiesKHR capabilities;
};

enum class VulkanCommandBufferState : uint8_t
{
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED
};

struct VulkanCommandBuffer 
{
    VkCommandBuffer handle;

    // Command buffer state.
    VulkanCommandBufferState state;
};

/**
* @brief Stores all the information related to the Vulkan Physical and Logical Device
*/
struct VulkanDevice 
{
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
    
    VkSwapChainSupportDetails swapChainSupport;
    VkSampleCountFlagBits msaaSamples;

    VkFormat colorFormat;
    VkFormat depthFormat;

    int32 graphicsQueueIndex;
    int32 presentQueueIndex;
    int32 computeQueueIndex;
    int32 transferQueueIndex;

    bool supportsDeviceLocalHostVisible;

    VkCommandPool mainGraphicsCommandPool;
    VkCommandPool mainTransferCommandPool;

    /* MULTITHREADING */
    std::mutex workerCommandPoolsMutex;
    std::unordered_map<std::thread::id, VkCommandPool> workerCommandPools;

    VkQueue graphicsQueue;
    std::mutex graphicsQueueMutex;

    VkQueue presentQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;
    std::mutex transferQueueMutex;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
};

struct VulkanShaderStage 
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    VkShaderModule handle;

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo;
};

struct VulkanPipeline
{
    VkPipeline handle;
    VkPipelineLayout pipelineLayout;
};

// Max number of simultaneously uploaded geometries
// TODO: make configurable
constexpr uint32 VULKAN_MAX_GEOMETRY_COUNT = 4096;

/**
 * @brief Internal buffer data for geometry.
 */
struct VulkanGeometryData
{
    uint32 ID;
    uint32 generation;

    uint32 vertexCount;
    uint32 vertexSize;
    uint64 vertexBufferOffset;

    uint32 indexCount;
    uint32 indexSize;
    uint64 indexBufferOffset;
};

struct VulkanImGuiResources
{
    // ---------- Editor Resources ---------- //
    VkDescriptorPool descriptorPool;

    // ---------- Scene Viewport Resources ---------- //
    // Framebuffers + descriptor sets are one-per-swapchain-image; both vectors are
    // sized to swapChainImages.size() at creation (CreateFramebuffers / Create*DescriptorSets).
    std::vector<VulkanImage> m_ViewportImages;
    VulkanImage m_ViewportDepthAttachment;
    std::vector<VkFramebuffer> m_ViewportFramebuffers;
    std::vector<VulkanCommandBuffer> m_ViewportCommandBuffers;

    VkSampler m_ViewportTextureSampler;
    std::vector<VkDescriptorSet> m_ViewportDescriptorSets;

    // ---------- Game Viewport Resources ---------- //
    std::vector<VulkanImage> m_GameViewportImages;
    VulkanImage m_GameViewportDepthAttachment;
    std::vector<VkFramebuffer> m_GameViewportFramebuffers;
    std::vector<VulkanCommandBuffer> m_GameViewportCommandBuffers;

    VkSampler m_GameViewportTextureSampler;
    std::vector<VkDescriptorSet> m_GameViewportDescriptorSets;

    // ---------- Pick (Mouse Picking) Resources ---------- //
    VulkanImage m_PickImage;
    VulkanImage m_PickDepthAttachment;
    VkFramebuffer m_PickFramebuffer;
};

struct VulkanSubmitTask {
    VkQueue queue;
    uint32_t submitCount;
    const VkSubmitInfo* pSubmits;
    VkFence fence;
    bool waitIdle;
    std::promise<bool> resultPromise;
};

/**
 * @brief Stores all the Vulkan Context variables
 */
struct VulkanContext
{
    float frameDeltaTime;

    int32 framebufferWidth;
    int32 framebufferHeight;

    // Current generation of framebuffer size. If it does not match framebuffer_size_last_generation,
    // a new one should be generated.
    uint64 framebufferSizeGeneration;
    // The generation of the framebuffer when it was last created. Set to framebuffer_size_generation
    // when updated.
    uint64 framebufferSizeLastGeneration;

	VkInstance instance;
	VkAllocationCallbacks* allocator;
	VkSurfaceKHR surface;

	VkDebugUtilsMessengerEXT debugMessenger;

	VulkanDevice device;

    VulkanSwapChain swapChain;

    RenderMode renderMode = RenderMode::EDITOR;

    VulkanRenderpass sceneRenderpass;
    VulkanRenderpass gameRenderpass;
    VulkanRenderpass uiRenderpass;
    VulkanRenderpass pickRenderpass;

    // GAME mode only: non-offscreen renderpass + framebuffers targeting swapchain directly.
    // One framebuffer per swapchain image; sized to swapChainImages.size() in CreateFramebuffers.
    VulkanRenderpass gameSwapchainRenderpass{};
    std::vector<VkFramebuffer> gameSwapchainFramebuffers{};

    VulkanBuffer objectVertexBuffer;
    VulkanBuffer objectIndexBuffer;

    std::vector<VulkanCommandBuffer> graphicsCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> queueCompleteSemaphores;

    // Sized at runtime in CreateSyncObjects (maxFramesInFlight / swapchain image count
    // respectively) — not fixed-size, so the engine adapts to swapchains that report
    // more than 3 images (e.g. Mesa llvmpipe in a headless/VM environment).
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    uint32 imageIndex;
    uint32 currentFrame;
    bool recreatingSwapchain;

    // ── Built-in shaders via the ResourceShader / VulkanShader system ─────────
    // Loaded in Initialize(); destroyed in Shutdown().
    // internalData points to a heap-allocated VulkanShader.
    class ResourceShader* builtInMaterialShader = nullptr;
    class ResourceShader* builtInGameShader     = nullptr;
    class ResourceShader* builtInPickShader     = nullptr;
    class ResourceShader* builtInOutlineShader  = nullptr; // Scene renderpass only; ResourceManager-owned
    class ResourceShader* builtInGridShader     = nullptr; // Scene renderpass only; ResourceManager-owned

    // ── Background gradient shader ─────────────────────────────────────────────
    class ResourceShader* builtInSceneBackgroundShader = nullptr; // Scene renderpass; ResourceManager-owned
    class ResourceShader* builtInGameBackgroundShader  = nullptr; // Game renderpass; VulkanBackend-owned clone

    // ── Editor grid resources ──────────────────────────────────────────────────
    VulkanBuffer gridVertexBuffer{};
    uint32       gridVertexCount = 0;

    // ── Bounding box shader ────────────────────────────────────────────────────
    class ResourceShader* builtInBoundingBoxShader = nullptr; // Scene renderpass; ResourceManager-owned

    // True once any wireframe debug draw has updated builtInBoundingBoxShader's
    // set=0 this frame. Reset when the SCENE renderpass begins recording. The
    // first wireframe draw updates set=0; the rest only rebind it (updating a
    // bound descriptor set would invalidate the command buffer). See the set=0
    // inheritance note in iRendererBackend.h.
    bool wireframeGlobalSetThisFrame = false;

    // ── Bounding box unit-cube wireframe (static, shared for all boxes) ────────
    VulkanBuffer boundingBoxVertexBuffer{};
    uint32       boundingBoxVertexCount = 0;

    // ── Camera frustum wireframe (dynamic, updated each frame) ─────────────────
    // Capacity: k_MaxCameraFrustums (8) × 24 vertices (12 edges × 2 endpoints)
    VulkanBuffer frustumVertexBuffer{};
    uint32       frustumVertexCapacity = 0; // in vertices

    // ── Point light debug sphere wireframe (static, shared for all lights) ─────
    // 3 great-circle rings (XY, XZ, YZ planes) as line lists; scaled per-draw.
    VulkanBuffer pointLightSphereVertexBuffer{};
    uint32       pointLightSphereVertexCount = 0;

    // ── Directional light debug pyramid wireframe (static, shared for all dir lights) ─
    // Narrow pyramid pointing in local -Y (8 edges = 16 line endpoints). LINE_LIST.
    VulkanBuffer dirLightPyramidVertexBuffer{};
    uint32       dirLightPyramidVertexCount = 0;

    // ── Spot light debug cone wireframe (static, shared for all spot lights) ──────
    // Apex at origin, base circle at y=-1 (24 segments + 24 spokes = 96 endpoints). LINE_LIST.
    VulkanBuffer spotLightConeVertexBuffer{};
    uint32       spotLightConeVertexCount = 0;

    // ── Per-frame instance SSBO (model matrices for GPU instancing) ────────────
    // One buffer per frame-in-flight (triple-buffered). Persistently mapped.
    // Layout: mat4[c_maxInstances] — indexed by gl_InstanceIndex in the shader.
    std::array<VulkanBuffer, 3> instanceSSBO{};
    std::array<void*, 3>        instanceSSBOMapped{};

    // TODO: make dynamic
    std::array<VulkanGeometryData, VULKAN_MAX_GEOMETRY_COUNT> geometries;
    std::mutex geometriesMutex;     // guards slot scan + ID assignment in CreateGeometry/DestroyGeometry
    std::mutex vertexBufferMutex;   // guards objectVertexBuffer freelist (Allocate/Free)
    std::mutex indexBufferMutex;    // guards objectIndexBuffer freelist (Allocate/Free)

    VulkanImGuiResources imGuiResources;

    std::deque<VulkanSubmitTask> submitQueue;
    std::mutex submitQueueMutex;

    bool isShuttingDown = false;

    // ── Injected dependencies (set before Initialize(), not owned) ─────────────
    IRenderWindow*                             window          = nullptr;
    EventSystem*                               eventSystem     = nullptr;
    nous::engine::multithreading::NOUS_JobSystem*        jobSystem       = nullptr;
    IRenderResourceProvider*                   resourceManager = nullptr;
};

struct VulkanTextureData 
{
    VulkanImage image;
    VkSampler sampler;
};

#endif // VULKANTYPES_INL
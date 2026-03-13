#ifndef VULKANTYPES_INL
#define VULKANTYPES_INL

#include "Engine/Core/Globals.h"
#include "Engine/Renderer/RendererTypes.h"

#include <vulkan/vulkan.h>

#include <future>
#include <array>
#include <deque>

class Freelist;

struct VulkanImage
{
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;

    uint32 width;
    uint32 height;
};

enum class VulkanRenderPassState 
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

    std::array<VkFramebuffer, 3> swapChainFramebuffers;
};

struct VkSwapChainSupportDetails
{
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
    VkSurfaceCapabilitiesKHR capabilities;
};

enum class VulkanCommandBufferState
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

    /* MULTITHREADING */
    std::unordered_map<uint32, VkCommandPool> workerCommandPools;

    VkQueue graphicsQueue;
    std::mutex graphicsQueueMutex;

    VkQueue presentQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;

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

struct VulkanDescriptorState
{
    // One per frame
    uint32 generations[3];
    uint32 ids[3];
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
    std::vector<VulkanImage> m_ViewportImages;
    VulkanImage m_ViewportDepthAttachment;
    std::array<VkFramebuffer, 3> m_ViewportFramebuffers;
    std::vector<VulkanCommandBuffer> m_ViewportCommandBuffers;

    VkSampler m_ViewportTextureSampler;
    std::array<VkDescriptorSet, 3> m_ViewportDescriptorSets;

    // ---------- Game Viewport Resources ---------- //
    std::vector<VulkanImage> m_GameViewportImages;
    VulkanImage m_GameViewportDepthAttachment;
    std::array<VkFramebuffer, 3> m_GameViewportFramebuffers;
    std::vector<VulkanCommandBuffer> m_GameViewportCommandBuffers;

    VkSampler m_GameViewportTextureSampler;
    std::array<VkDescriptorSet, 3> m_GameViewportDescriptorSets;

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

    VulkanRenderpass sceneRenderpass;
    VulkanRenderpass gameRenderpass;
    VulkanRenderpass uiRenderpass;
    VulkanRenderpass pickRenderpass;

    VulkanBuffer objectVertexBuffer;
    VulkanBuffer objectIndexBuffer;

    std::vector<VulkanCommandBuffer> graphicsCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> queueCompleteSemaphores;

    std::array<VkFence, 2> inFlightFences;
    std::array<VkFence, 3> imagesInFlight;

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

    // TODO: make dynamic
    std::array<VulkanGeometryData, VULKAN_MAX_GEOMETRY_COUNT> geometries;
    std::mutex geometriesMutex;     // guards slot scan + ID assignment in CreateGeometry/DestroyGeometry
    std::mutex vertexBufferMutex;   // guards objectVertexBuffer freelist (Allocate/Free)
    std::mutex indexBufferMutex;    // guards objectIndexBuffer freelist (Allocate/Free)

    VulkanImGuiResources imGuiResources;

    std::deque<VulkanSubmitTask> submitQueue;
    std::mutex submitQueueMutex;
    std::condition_variable submitQueueCV;

    bool isShuttingDown = false;
};

struct VulkanTextureData 
{
    VulkanImage image;
    VkSampler sampler;
};

#endif // VULKANTYPES_INL
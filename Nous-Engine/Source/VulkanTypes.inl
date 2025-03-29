#pragma once

#include "Globals.h"
#include "RendererTypes.inl"

#include "ResourceMaterial.h"
#include "ResourceMesh.h"
#include "ResourceTexture.h"

#include "Vulkan.h"
#include "FreeList.h"

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

    float4 renderArea;
    float4 clearColor;

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

    VkCommandPool graphicsCommandPool;

    VkQueue graphicsQueue;
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

#pragma region MATERIAL_SHADER

constexpr uint32 VULKAN_MATERIAL_SHADER_STAGE_COUNT = 2;
constexpr uint32 VULKAN_MATERIAL_SHADER_DESCRIPTOR_COUNT = 2;
constexpr uint32 VULKAN_MATERIAL_SHADER_SAMPLER_COUNT = 1;

// Max number of material instances
// TODO: make configurable
constexpr uint32 VULKAN_MAX_MATERIAL_COUNT = 1024;

struct VulkanMaterialShaderInstanceState 
{
    // Per frame
    std::array<VkDescriptorSet, 3> descriptorSets;
    // Per descriptor
    std::array<VulkanDescriptorState, VULKAN_MATERIAL_SHADER_DESCRIPTOR_COUNT> descriptorStates;
};

struct VulkanMaterialShaderGlobalUBO
{
    float4x4 projection;   // 64 bytes
    float4x4 view;         // 64 bytes

    float4x4 m_reserved0;  // 64 bytes, reserved for future use
    float4x4 m_reserved1;  // 64 bytes, reserved for future use
};

struct VulkanMaterialShaderInstanceUBO
{
    float4 diffuseColor;   // 16 bytes

    float4 v_reserved0;     // 16 bytes, reserved for future use
    float4 v_reserved1;     // 16 bytes, reserved for future use
    float4 v_reserved2;     // 16 bytes, reserved for future use

    float4x4 m_reserved0;  // 64 bytes, reserved for future use
    float4x4 m_reserved1;  // 64 bytes, reserved for future use
    float4x4 m_reserved2;  // 64 bytes, reserved for future use
};

struct VulkanMaterialShader 
{
    // Vertex and Fragment Stages
    std::array<VulkanShaderStage, VULKAN_MATERIAL_SHADER_STAGE_COUNT> stages;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalDescriptorSetLayout;

    // One descriptor set per frame - max 3 for triple-buffering.
    std::array<VkDescriptorSet, 3> globalDescriptorSets;

    // Global uniform object.
    VulkanMaterialShaderGlobalUBO globalUBO;

    // Global uniform buffer.
    VulkanBuffer globalUniformBuffer;

    VkDescriptorPool localDescriptorPool;
    VkDescriptorSetLayout localDescriptorSetLayout;

    // Local uniform buffers.
    VulkanBuffer localUniformBuffer;
    // TODO: manage a free list of some kind here instead.
    uint32 localUniformBufferIndex;

    std::array<TextureMapType, VULKAN_MATERIAL_SHADER_SAMPLER_COUNT> samplerUsage;

    // TODO: make dynamic
    std::array<VulkanMaterialShaderInstanceState, VULKAN_MAX_MATERIAL_COUNT> instanceStates;

    VulkanPipeline pipeline;
};

#pragma endregion

#pragma region UI_SHADER

constexpr uint32 VULKAN_UI_SHADER_STAGE_COUNT = 2;
constexpr uint32 VULKAN_UI_SHADER_DESCRIPTOR_COUNT = 2;
constexpr uint32 VULKAN_UI_SHADER_SAMPLER_COUNT = 1;

// Max number of ui control instances.
// TODO: make configurable.
constexpr uint32 VULKAN_MAX_UI_COUNT = 1024;

struct VulkanUIShaderInstanceState
{
    // Per frame
    std::array<VkDescriptorSet, 3> descriptorSets;
    // Per descriptor
    std::array<VulkanDescriptorState, VULKAN_UI_SHADER_DESCRIPTOR_COUNT> descriptorStates;
};

struct VulkanUIShaderGlobalUBO
{
    float4x4 projection;   // 64 bytes
    float4x4 view;         // 64 bytes

    float4x4 m_reserved0;  // 64 bytes, reserved for future use
    float4x4 m_reserved1;  // 64 bytes, reserved for future use
};

struct VulkanUIShaderInstanceUBO
{
    float4 diffuseColor;   // 16 bytes

    float4 v_reserved0;     // 16 bytes, reserved for future use
    float4 v_reserved1;     // 16 bytes, reserved for future use
    float4 v_reserved2;     // 16 bytes, reserved for future use

    float4x4 m_reserved0;  // 64 bytes, reserved for future use
    float4x4 m_reserved1;  // 64 bytes, reserved for future use
    float4x4 m_reserved2;  // 64 bytes, reserved for future use
};

struct VulkanUIShader
{
    // Vertex and Fragment Stages
    std::array<VulkanShaderStage, VULKAN_UI_SHADER_STAGE_COUNT> stages;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalDescriptorSetLayout;

    // One descriptor set per frame - max 3 for triple-buffering.
    std::array<VkDescriptorSet, 3> globalDescriptorSets;

    // Global uniform object.
    VulkanMaterialShaderGlobalUBO globalUBO;

    // Global uniform buffer.
    VulkanBuffer globalUniformBuffer;

    VkDescriptorPool localDescriptorPool;
    VkDescriptorSetLayout localDescriptorSetLayout;

    // Local uniform buffers.
    VulkanBuffer localUniformBuffer;
    // TODO: manage a free list of some kind here instead.
    uint32 localUniformBufferIndex;

    std::array<TextureMapType, VULKAN_UI_SHADER_SAMPLER_COUNT> samplerUsage;

    // TODO: make dynamic
    std::array<VulkanUIShaderInstanceState, VULKAN_MAX_UI_COUNT> instanceStates;

    VulkanPipeline pipeline;
};

#pragma endregion

struct VulkanImGuiResources
{
    VkDescriptorPool descriptorPool;

    std::vector<VulkanImage> viewportImages;

    VulkanRenderpass viewportRenderPass;
    VulkanPipeline viewportPipeline;

    std::array<VkFramebuffer, 3> viewportFramebuffers;

    VkCommandPool viewportCommandPool;
    std::vector<VulkanCommandBuffer> viewportCommandBuffers;

    // -------------------- //

    VkSampler textureSampler;
    std::vector<VkDescriptorSet> descriptorSets;
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
    VulkanRenderpass mainRenderpass;
    VulkanRenderpass uiRenderpass;

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

    VulkanMaterialShader materialShader;
    VulkanUIShader uiShader;

    // TODO: make dynamic
    std::array<VulkanGeometryData, VULKAN_MAX_GEOMETRY_COUNT> geometries;

    // Multiple Render Passes
    std::array<VkFramebuffer, 3> worldFramebuffers;

    VulkanImGuiResources imGuiResources;
};

struct VulkanTextureData 
{
    VulkanImage image;
    VkSampler sampler;
};
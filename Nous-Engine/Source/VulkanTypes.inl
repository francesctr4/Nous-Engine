#pragma once

#include "Globals.h"
#include "RendererTypes.inl"

#include "Vulkan.h"

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

    float x, y, w, h;
    float r, g, b, a;

    float depth;
    uint32 stencil;

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
};

struct VulkanFramebuffer 
{
    VkFramebuffer handle;
    std::vector<VkImageView> attachments;
    VulkanRenderpass* renderpass;
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

    std::vector<VulkanFramebuffer> swapChainFramebuffers;
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

    VkCommandPool graphicsCommandPool;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
};

struct VulkanFence 
{
    VkFence handle;
    bool isSignaled;
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
};

constexpr uint32 VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT = 2;
struct VulkanObjectShaderLocalState 
{
    // Per frame
    std::array<VkDescriptorSet, 3> descriptorSets;
    // Per descriptor
    std::array<VulkanDescriptorState, VULKAN_OBJECT_SHADER_DESCRIPTOR_COUNT> descriptorStates;
};

// Max number of objects
constexpr uint32 VULKAN_OBJECT_SHADER_MAX_OBJECT_COUNT = 1024;

constexpr uint32 VULKAN_OBJECT_SHADER_STAGE_COUNT = 2;
struct VulkanObjectShader 
{
    // Vertex and Fragment Stages
    std::array<VulkanShaderStage, VULKAN_OBJECT_SHADER_STAGE_COUNT> stages;

    VkDescriptorPool globalDescriptorPool;
    VkDescriptorSetLayout globalDescriptorSetLayout;

    // One descriptor set per frame - max 3 for triple-buffering.
    std::array<VkDescriptorSet, 3> globalDescriptorSets;

    // Global uniform object.
    GlobalUniformObject globalUBO;

    // Global uniform buffer.
    VulkanBuffer globalUniformBuffer;

    VkDescriptorPool localDescriptorPool;
    VkDescriptorSetLayout localDescriptorSetLayout;

    // Local uniform buffers.
    VulkanBuffer localUniformBuffer;
    // TODO: manage a free list of some kind here instead.
    uint32 localUniformBufferIndex;
    // TODO: make dynamic
    std::array<VulkanObjectShaderLocalState, VULKAN_OBJECT_SHADER_MAX_OBJECT_COUNT> localObjectStates;

    // Pointers to default textures.
    Texture* defaultDiffuse;

    VulkanPipeline pipeline;
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

    VulkanBuffer objectVertexBuffer;
    uint64 geometryVertexOffset;

    VulkanBuffer objectIndexBuffer;
    uint64 geometryIndexOffset;

    std::vector<VulkanCommandBuffer> graphicsCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> queueCompleteSemaphores;

    std::vector<VulkanFence> inFlightFences;

    // Holds pointers to fences which exist and are owned elsewhere.
    std::vector<VulkanFence*> imagesInFlight;

    uint32 imageIndex;
    uint32 currentFrame;
    bool recreatingSwapchain;

    VulkanObjectShader objectShader;
};

struct VulkanTextureData 
{
    VulkanImage image;
    VkSampler sampler;
};
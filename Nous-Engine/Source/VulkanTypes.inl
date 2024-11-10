#pragma once

#include "Globals.h"

#include "Vulkan.h"
#include "SDL2.h"

#include "DynamicArray.h"

/**
 * @brief Checks the given expression's return value against VK_SUCCESS.
 */
#define VK_CHECK(expr)					\
{										\
	NOUS_ASSERT(expr == VK_SUCCESS);	\
}										

 /**
  * @brief Checks the given expression's return value against VK_SUCCESS.
  */
#define VK_CHECK_MSG(expr, message)					\
{													\
	NOUS_ASSERT_MSG(expr == VK_SUCCESS, message);	\
}	

const uint8 MAX_FRAMES_IN_FLIGHT = 2;

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

    float32 x, y, w, h;
    float32 r, g, b, a;

    float32 depth;
    uint32 stencil;

    VulkanRenderPassState state;
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
    COMMAND_BUFFER_STATE_READY,
    COMMAND_BUFFER_STATE_RECORDING,
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,
    COMMAND_BUFFER_STATE_RECORDING_ENDED,
    COMMAND_BUFFER_STATE_SUBMITTED,
    COMMAND_BUFFER_STATE_NOT_ALLOCATED
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

/**
 * @brief Stores all the Vulkan Context variables
 */
struct VulkanContext 
{
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

    std::vector<VulkanCommandBuffer> graphicsCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> queueCompleteSemaphores;

    std::vector<VulkanFence> inFlightFences;

    // Holds pointers to fences which exist and are owned elsewhere.
    std::vector<VulkanFence*> imagesInFlight;

    uint32 imageIndex;
    uint32 currentFrame;
    bool recreatingSwapchain;
};
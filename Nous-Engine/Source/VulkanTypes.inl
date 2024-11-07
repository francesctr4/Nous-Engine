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

struct VulkanSwapChain
{
    VkSwapchainKHR handle;
    
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VulkanImage depthAttachment;
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
    VkFormat depthFormat;

    int32 graphicsQueueIndex;
    int32 presentQueueIndex;
    int32 computeQueueIndex;
    int32 transferQueueIndex;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
};

/**
 * @brief Stores all the Vulkan Context variables
 */
struct VulkanContext 
{
    uint32 framebufferWidth;
    uint32 framebufferHeight;

	VkInstance instance;
	VkAllocationCallbacks* allocator;
	VkSurfaceKHR surface;

	VkDebugUtilsMessengerEXT debugMessenger;

	VulkanDevice device;

    VulkanSwapChain swapChain;
    VulkanRenderpass mainRenderpass;

    uint32 imageIndex;
    uint32 currentFrame;
    bool recreatingSwapchain;
};
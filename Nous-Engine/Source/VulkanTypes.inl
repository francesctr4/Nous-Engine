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

/**
* @brief Stores all the information related to the Vulkan Physical and Logical Device
*/
struct VulkanDevice 
{
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
    
    VkSwapChainSupportDetails swapChainSupport;
    VkSampleCountFlagBits msaaSamples;

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

    uint32 imageIndex;
    uint32 currentFrame;
    bool recreatingSwapchain;

    int32 FindMemoryIndex(uint32 typeFilter, uint32 propertyFlags);
};

int32 VulkanContext::FindMemoryIndex(uint32 typeFilter, uint32 propertyFlags)
{
    return int32();
}
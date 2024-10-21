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

const int MAX_FRAMES_IN_FLIGHT = 2;

struct VulkanSwapChain
{
    VkSwapchainKHR swapChainHandle;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
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
	VkInstance instance;
	VkAllocationCallbacks* allocator;
	VkSurfaceKHR surface;

	VkDebugUtilsMessengerEXT debugMessenger;

	VulkanDevice device;

    VulkanSwapChain swapChain;
};
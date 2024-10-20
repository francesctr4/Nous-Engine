#pragma once

#include "Globals.h"

#include "Vulkan.h"
#include "SDL2.h"

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

/**
* @brief Stores both the Physical and Logical Vulkan Device
*/
struct VulkanDevice 
{
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
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
};
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
 * @brief Stores all the Vulkan-specific variables
 */
struct VulkanContext 
{
	VkInstance instance;
	VkAllocationCallbacks* allocator;
	VkDebugUtilsMessengerEXT debugMessenger;
};
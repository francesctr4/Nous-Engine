#pragma once

#include "Globals.h"

#include "Vulkan.h"
#include "SDL2.h"

/**
 * @brief Stores all the Vulkan-specific variables
 */
struct VulkanContext 
{
	VkInstance instance;
	VkAllocationCallbacks* allocator;
};
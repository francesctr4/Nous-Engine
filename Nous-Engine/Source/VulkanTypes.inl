#pragma once

#include "Vulkan.h"

struct VulkanContext 
{
	VkInstance instance;
	VkAllocationCallbacks* allocator;
};
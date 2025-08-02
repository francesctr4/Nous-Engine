#ifndef VULKANSYNCOBJECTS_H
#define VULKANSYNCOBJECTS_H

#include "Renderer/Vulkan/VulkanTypes.inl"

namespace NOUS_VulkanSyncObjects 
{
	bool CreateSyncObjects(VulkanContext* vkContext);
	void DestroySyncObjects(VulkanContext* vkContext);
}

#endif // VULKANSYNCOBJECTS_H
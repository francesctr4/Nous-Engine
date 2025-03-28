#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanSyncObjects 
{
	bool CreateSyncObjects(VulkanContext* vkContext);
	void DestroySyncObjects(VulkanContext* vkContext);
}
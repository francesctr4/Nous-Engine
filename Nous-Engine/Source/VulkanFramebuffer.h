#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanFramebuffer 
{
	bool CreateFramebuffers(VulkanContext* vkContext);

	void DestroyFramebuffers(VulkanContext* vkContext);
}
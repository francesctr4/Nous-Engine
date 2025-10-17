#ifndef VULKANFRAMEBUFFER_H
#define VULKANFRAMEBUFFER_H

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"

namespace NOUS_VulkanFramebuffer 
{
	bool CreateFramebuffers(VulkanContext* vkContext);
	void DestroyFramebuffers(VulkanContext* vkContext);
}

#endif // VULKANFRAMEBUFFER_H
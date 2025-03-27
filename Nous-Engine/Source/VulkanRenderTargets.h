#pragma once

#include "VulkanTypes.inl"

namespace NOUS_VulkanRenderTargets
{
	bool CreateOffscreenRenderTarget(VulkanContext* vkContext);

	void BindOffscreenRenderTarget(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer);

	void UnbindOffscreenRenderTarget(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer);

	bool RecreateOffscreenRenderTarget(VulkanContext* vkContext);

	void DestroyOffscreenRenderTarget(VulkanContext* vkContext);
}
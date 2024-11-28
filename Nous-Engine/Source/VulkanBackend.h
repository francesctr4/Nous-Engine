#pragma once

#include "RendererBackend.h"

// --------------- Vulkan Renderer Backend --------------- \\

struct VulkanContext;

class VulkanBackend : public IRendererBackend 
{
public:

	VulkanBackend();
	virtual ~VulkanBackend() override;

	bool Initialize() override;
	void Shutdown() override;

	void Resized(uint16 width, uint16 height) override;

	bool BeginFrame(float dt) override;
	bool EndFrame(float dt) override;

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode) override;
	void UpdateObject(float4x4 model);

	// ------------------------------------------------ //

	bool RecreateResources();

private:

	static VulkanContext* vkContext;

};
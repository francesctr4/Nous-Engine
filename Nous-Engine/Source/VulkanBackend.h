#pragma once

#include "RendererBackend.h"

struct VulkanContext;

class VulkanBackend : public IRendererBackend 
{
public:

	VulkanBackend();
	virtual ~VulkanBackend() override;

	bool Initialize() override;
	void Shutdown() override;

	void Resized(uint16 width, uint16 height) override;

	bool BeginFrame(float32 dt) override;
	bool EndFrame(float32 dt) override;

	// ------------- Vulkan Specific Functions ------------- \\

	bool CreateInstance();

private:

	static VulkanContext* vkContext;

};
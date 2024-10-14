#pragma once

#include "RendererBackend.h"
#include "VulkanTypes.inl"

class VulkanBackend : public IRendererBackend 
{
public:

	VulkanBackend();
	virtual ~VulkanBackend();

	bool Initialize() override;
	void Shutdown() override;

	void Resized(uint16 width, uint16 height) override;

	bool BeginFrame(float32 dt) override;
	bool EndFrame(float32 dt) override;

	// ---------------------- \\

	bool CreateInstance();

private:

	static VulkanContext* vkContext;

};
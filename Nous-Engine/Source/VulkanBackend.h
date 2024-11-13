#pragma once

#include "RendererBackend.h"
#include "VulkanTypes.inl"

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

	bool RecreateResources();

	// ------------------------------------ Vulkan Pipeline Functions ------------------------------------ \\

	bool CreateInstance();
	bool CreateSurface();

	// ------------------------------------ Vulkan Helper Functions ------------------------------------ \\
	
	// --------------- Validation Layers --------------- \\

	bool CheckValidationLayerSupport(const std::array<const char*, c_VALIDATION_LAYERS_COUNT>& validationLayers);

	// --------------- Extensions --------------- \\

	void ShowSupportedExtensions();
	std::vector<const char*> GetRequiredExtensions();

private:

	static VulkanContext* vkContext;

};
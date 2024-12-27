#pragma once

#include "RendererBackend.h"

#include "ResourceTypes.inl"

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

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode) override;
	void UpdateObject(GeometryRenderData renderData) override;

	// ----------------------------------------------------------------------------------------------- //
	// TEMPORAL //

	void CreateTexture(const char* path, int32 width, int32 height, 
		int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture) override;

	void DestroyTexture(Texture* texture) override;

	static VulkanContext* GetVulkanContext();

private:

	static VulkanContext* vkContext;

};
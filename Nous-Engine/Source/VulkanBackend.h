#pragma once

#include "RendererBackend.h"

#include "ResourceTypes.inl"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

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

	bool BeginRenderpass(BuiltInRenderpass renderpassID) override;
	bool EndRenderpass(BuiltInRenderpass renderpassID) override;

	bool RecreateResources();

	void UpdateGlobalWorldState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode) override;
	void UpdateGlobalUIState(float4x4 projection, float4x4 view, int32 mode) override;

	void DrawGeometry(GeometryRenderData renderData) override;

	// ----------------------------------------------------------------------------------------------- //
	// TEMPORAL //

	void CreateTexture(const uint8* pixels, ResourceTexture* outTexture) override;
	void DestroyTexture(ResourceTexture* texture) override;

	bool CreateMaterial(ResourceMaterial* material) override;
	void DestroyMaterial(ResourceMaterial* material) override;

	bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* geometry) override;
	void DestroyGeometry(ResourceMesh* geometry) override;

	static VulkanContext* GetVulkanContext();

private:

	static VulkanContext* vkContext;

};
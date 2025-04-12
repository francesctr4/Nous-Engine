#pragma once

#include "RendererTypes.inl"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

class RendererBackend
{
public:

	RendererBackend();
	virtual ~RendererBackend();

	bool Create(RendererBackendType bType);
	void Destroy();

	bool Initalize();
	void Shutdown();

	void Resized(uint16 width, uint16 height);

	bool BeginFrame(float dt);
	bool EndFrame(float dt);

	bool BeginRenderpass(BuiltInRenderpass renderpassID);
	bool EndRenderpass(BuiltInRenderpass renderpassID);

	void UpdateGlobalWorldState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode);
	void UpdateGlobalUIState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, int32 mode);

	void DrawGeometry(BuiltInRenderpass renderpassID, GeometryRenderData renderData);

	void CreateTexture(const uint8* pixels, ResourceTexture* outTexture);
	void DestroyTexture(ResourceTexture* texture);

	bool CreateMaterial(ResourceMaterial* material);
	void DestroyMaterial(ResourceMaterial* material);

	bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* outGeometry);
	void DestroyGeometry(ResourceMesh* geometry);

	// -------------------------------------- \\

	uint64 frameNumber;

private:
	
	IRendererBackend* backendInterface;

};
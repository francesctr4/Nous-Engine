#pragma once

#include "RendererTypes.inl"

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

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode);
	void DrawGeometry(GeometryRenderData renderData);

	void CreateTexture(const uint8* pixels, Texture* outTexture);
	void DestroyTexture(Texture* texture);

	bool CreateMaterial(Material* material);
	void DestroyMaterial(Material* material);

	bool CreateGeometry(uint32 vertexCount, const Vertex* vertices, uint32 indexCount, const uint32* indices, Geometry* outGeometry);
	void DestroyGeometry(Geometry* geometry);

	// -------------------------------------- \\

	uint64 frameNumber;

private:
	
	IRendererBackend* backendInterface;

};
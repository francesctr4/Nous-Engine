#pragma once

#include "RendererTypes.inl"

class RendererBackend;

class RendererFrontend
{
public:

	RendererFrontend();
	virtual ~RendererFrontend();

	bool Initialize(RendererBackendType backendType);
	void Shutdown();

	void OnResized(uint16 width, uint16 height);

	bool DrawFrame(RenderPacket* packet);

	void CreateTexture(const uint8* pixels, Texture* outTexture);
	void DestroyTexture(Texture* texture);

	bool CreateMaterial(Material* material);
	void DestroyMaterial(Material* material);

	bool CreateGeometry(uint32 vertexCount, const Vertex* vertices, uint32 indexCount, const uint32* indices, Geometry* outGeometry);
	void DestroyGeometry(Geometry* geometry);

private:

	bool BeginFrame(float dt);
	bool EndFrame(float dt);

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode);
	void DrawGeometry(GeometryRenderData renderData);

public:

	// TODO: temporary
	Material* testMaterial;
	// TODO: end temporary

	RendererBackendType backendType;

private:

	RendererBackend* backend;

};
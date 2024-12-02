#pragma once

#include "RendererTypes.inl"

class RendererBackend;

class RendererFrontend
{
public:

	RendererFrontend();
	virtual ~RendererFrontend();

	bool Initialize();
	void Shutdown();

	void OnResized(uint16 width, uint16 height);

	bool DrawFrame(RenderPacket* packet);

	void InitTexture(Texture* texture);
	void CreateTexture(const char* path, bool autoRelease, int32 width, int32 height,
		int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture);
	void DestroyTexture(Texture* texture);

private:

	bool BeginFrame(float dt);
	bool EndFrame(float dt);

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode);
	void UpdateObject(GeometryRenderData renderData);

public:

	// TODO: temporary
	Texture testDiffuse;
	// TODO: end temporary

private:

	void CreateDefaultTexture();

private:

	RendererBackend* backend;

	Texture defaultTexture;

};
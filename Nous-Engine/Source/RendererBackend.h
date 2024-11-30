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
	void UpdateObject(float4x4 model);

	void CreateTexture(const char* path, bool autoRelease, int32 width, int32 height,
		int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture);
	void DestroyTexture(Texture* texture);

	// -------------------------------------- \\

	uint64 frameNumber;

private:
	
	IRendererBackend* backendInterface;

};
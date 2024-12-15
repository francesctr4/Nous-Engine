#pragma once

#include "RendererBackend.h"

struct OpenGLContext;

class OpenGLBackend : public IRendererBackend
{
public:

	OpenGLBackend();
	virtual ~OpenGLBackend() override;

	bool Initialize() override;
	void Shutdown() override;

	void Resized(uint16 width, uint16 height) override;

	bool BeginFrame(float dt) override;
	bool EndFrame(float dt) override;

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode) override;
	void UpdateObject(GeometryRenderData renderData) override;

	void CreateTexture(const char* path, int32 width, int32 height,
		int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture) override;

	void DestroyTexture(Texture* texture) override;

	// ------------- OpenGL Specific Functions ------------- \\

private:

	static OpenGLContext* glContext;

};
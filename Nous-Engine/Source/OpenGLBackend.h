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

	// ------------- OpenGL Specific Functions ------------- \\

private:

	static OpenGLContext* glContext;

};
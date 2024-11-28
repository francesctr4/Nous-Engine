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

private:

	bool BeginFrame(float dt);
	bool EndFrame(float dt);

	void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode);

	static RendererBackend* backend;

};
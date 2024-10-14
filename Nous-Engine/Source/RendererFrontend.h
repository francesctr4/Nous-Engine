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

	bool BeginFrame(float32 dt);
	bool EndFrame(float32 dt);

	static RendererBackend* backend;

};
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

	bool BeginFrame(float32 dt);
	bool EndFrame(float32 dt);

	// -------------------------------------- \\

	uint64 frameNumber;

private:
	
	IRendererBackend* backendInterface;

};
#pragma once

#include "Globals.h"

/**
 * @brief Interface to implement by all the Renderer Backends
 */
struct IRendererBackend 
{
    virtual ~IRendererBackend() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void Resized(uint16 width, uint16 height) = 0;
    virtual bool BeginFrame(float32 dt) = 0;
    virtual bool EndFrame(float32 dt) = 0;
};

enum class RendererBackendType 
{
	VULKAN,
	OPENGL,
	DIRECTX
};

struct RenderPacket 
{
	float32 deltaTime;
};
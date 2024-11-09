#include "RendererFrontend.h"
#include "RendererBackend.h"

#include "MemoryManager.h"
#include "Logger.h"

RendererBackend* RendererFrontend::backend = nullptr;

RendererFrontend::RendererFrontend()
{
	backend = NOUS_NEW<RendererBackend>(MemoryManager::MemoryTag::RENDERER);
}

RendererFrontend::~RendererFrontend()
{
	NOUS_DELETE(backend, MemoryManager::MemoryTag::RENDERER);
}

bool RendererFrontend::Initialize()
{
	bool ret = true;

	// TODO: Make this configurable
	backend->Create(RendererBackendType::VULKAN);
	backend->frameNumber = 0;

	if (!backend->Initalize()) 
	{
		NOUS_FATAL("Renderer backend failed to initialize. Shutting down.");
		ret = false;
	}

	return ret;
}

void RendererFrontend::Shutdown()
{
	backend->Shutdown();
}

void RendererFrontend::OnResized(uint16 width, uint16 height)
{
	if (backend) 
	{
		backend->Resized(width, height);
	}
	else 
	{
		NOUS_WARN("Renderer Backend does not exist yet to accept resizing: %i %i", width, height);
	}
}

bool RendererFrontend::BeginFrame(float32 dt)
{
	return backend->BeginFrame(dt);
}

bool RendererFrontend::EndFrame(float32 dt)
{
	bool result = backend->EndFrame(dt);
	backend->frameNumber++;

	return result;
}

bool RendererFrontend::DrawFrame(RenderPacket* packet)
{
	bool ret = true;

	// If the begin frame returned successfully, mid-frame operations may continue.
	if (BeginFrame(packet->deltaTime)) 
	{
		// End of the frame. If this fails, it is likely unrecoverable.
		bool result = EndFrame(packet->deltaTime);

		if (!result) 
		{
			NOUS_ERROR("RendererFrontend::EndFrame() failed. Application shutting down...");
			ret = false;
		}
	}

	return ret;
}

#include "RendererFrontend.h"
#include "RendererBackend.h"

#include "MemoryManager.h"
#include "Logger.h"

RendererBackend* RendererFrontend::backend = nullptr;

bool RendererFrontend::Initialize()
{
	bool ret = true;

	backend = NOUS_NEW<RendererBackend>(MemoryManager::MemoryTag::RENDERER);

	// TODO: Make this configurable
	backend->Create(RendererBackendType::VULKAN);

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
	NOUS_DELETE(backend, MemoryManager::MemoryTag::RENDERER);
}

#include "RendererBackend.h"

bool RendererBackend::Create(RendererBackendType bType)
{
	bool ret = false;

	if (bType == RendererBackendType::VULKAN) 
	{
		// TODO
		ret = true;
	}

	return ret;
}

void RendererBackend::Destroy()
{

}

bool RendererBackend::Initalize()
{
	return false;
}

void RendererBackend::Shutdown()
{

}

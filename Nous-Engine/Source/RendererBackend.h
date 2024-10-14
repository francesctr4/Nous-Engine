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

private:


};
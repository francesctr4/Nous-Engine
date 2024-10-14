#pragma once

class RendererBackend;

class RendererFrontend
{
public:

	RendererFrontend();
	virtual ~RendererFrontend();

	bool Initialize();
	void Shutdown();

private:

	static RendererBackend* backend;

};
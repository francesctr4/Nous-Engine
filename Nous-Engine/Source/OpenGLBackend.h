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

	// ------------- OpenGL Specific Functions ------------- \\

private:

	static OpenGLContext* glContext;

};
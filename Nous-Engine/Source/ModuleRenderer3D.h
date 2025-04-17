#pragma once

#include "Module.h"

#include "RendererTypes.inl"

class RendererFrontend;

class ModuleRenderer3D : public Module
{
public:

	ModuleRenderer3D(Application* app);
	virtual ~ModuleRenderer3D();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

public:

	static RendererFrontend* rendererFrontend;

};
#pragma once

#include "Module.h"

union SDL_Event;
struct VulkanContext;
enum class RendererBackendType;

class ModuleEditor : public Module
{
public:

	// Constructor
	ModuleEditor(Application* app, std::string name, bool start_enabled = true);

	// Destructor
	virtual ~ModuleEditor();

	bool Awake() override;
	bool CleanUp() override;

	void ProcessInputEvent(const SDL_Event* event);
	void DrawEditor();

private:

	void InitFrame(RendererBackendType backendType);
	void InternalDrawEditor();
	void EndFrame(RendererBackendType backendType);

	// Vulkan Specific
	static VulkanContext* GetVulkanContext();

private:

	RendererBackendType currentBackendType;

};
#pragma once

#include "Module.h"
#include "External/ImGui/imgui.h"
#include "External/ImGui/backends/imgui_impl_sdl2.h"
#include "External/ImGui/backends/imgui_impl_vulkan.h"
#include "RendererTypes.inl"

union SDL_Event;
struct VulkanContext;

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

	RendererBackendType currentBackendType;

	void InitFrame(RendererBackendType backendType);
	void InternalDrawEditor();
	void EndFrame(RendererBackendType backendType);

	// Vulkan Specific
	static VulkanContext* GetVulkanContext();

};
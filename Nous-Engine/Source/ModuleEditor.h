#pragma once

#include "Module.h"
#include "External/ImGui/imgui.h"
#include "External/ImGui/backends/imgui_impl_sdl2.h"
#include "External/ImGui/backends/imgui_impl_vulkan.h"

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
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

	void ProcessImGuiEvent(const SDL_Event* event);
	void ImGuiNewFrame();

	static void InitFrame();
	static void DrawEditor();
	static VulkanContext* GetVulkanContext();
	static void EndFrame(VkCommandBuffer commandBuffer);
};
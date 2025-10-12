#ifndef MODULEEDITOR_H
#define MODULEEDITOR_H

#include "Core/Module.h"

#include <vector>
#include <memory>
#include <string>

union SDL_Event;
struct VulkanContext;
enum class RendererBackendType;
class IEditorWindow;
struct ImGuiIO;
struct ImFont;

class ModuleEditor : public Module
{
public:

	// Constructor
	ModuleEditor(Application* app);

	// Destructor
	virtual ~ModuleEditor();

	bool Awake() override;
	bool CleanUp() override;

	void ProcessInputEvent(const SDL_Event* event);
	void DrawEditor();

	// Array to store ImFont pointers
	static std::vector<ImFont*> fonts;

	IEditorWindow* GetEditorWindowByName(std::string name);

private:

	void InitFrame(RendererBackendType backendType);
	void InternalDrawEditor();
	void EndFrame(RendererBackendType backendType);

	// Vulkan Specific
	static VulkanContext* GetVulkanContext();

	void AddEditorWindow(std::unique_ptr<IEditorWindow> editorWindow);

private:

	RendererBackendType currentBackendType;

	std::vector<std::unique_ptr<IEditorWindow>> editorWindows;

};

#endif // MODULEEDITOR_H

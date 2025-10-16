#ifndef MODULEEDITOR_H
#define MODULEEDITOR_H

#include "Editor/EditorExport.h"

#include "Engine/Core/Module.h"
#include "Engine/Systems/Event System/IEventListener.h"
#include "Engine/Renderer/Frontend/IEditorOverlay.h"

#include <vector>
#include <memory>
#include <string>

union SDL_Event;
struct VulkanContext;
enum class RendererBackendType;
struct ImGuiIO;
struct ImFont;
class IEditorWindow;

class ModuleEditor : public Module, public IEditorOverlay, public IEventListener
{
public:

	// Constructor
	NOUS_EDITOR_API ModuleEditor(Application* app);

	// Destructor
	NOUS_EDITOR_API virtual ~ModuleEditor();

	NOUS_EDITOR_API bool Awake() override;
    NOUS_EDITOR_API bool Start() override;
	NOUS_EDITOR_API bool CleanUp() override;
	NOUS_EDITOR_API void DrawEditor() override;
	NOUS_EDITOR_API void OnEvent(const Event& event) override;

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

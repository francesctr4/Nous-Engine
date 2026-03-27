#ifndef MODULEEDITOR_H
#define MODULEEDITOR_H

#include "Editor/EditorExport.h"

#include "Engine/Modules/Module.h"
#include "Engine/Renderer/Frontend/IEditorOverlay.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Editor/EditorContext.h"

#include "Engine/Utils/DataStructures/NOUS_Vector.h"

#include <memory>
#include <string>

union SDL_Event;
struct VulkanContext;
enum class RendererBackendType;
struct ImGuiIO;
struct ImFont;
class IEditorWindow;

// Dependency Injection
class ModuleWindow;
class ModuleInput;
class ModuleCamera3D;
class ModuleResourceManager;
class ModuleScene;
class ModuleRenderer3D;

class ModuleEditor : public Module, public IEditorOverlay, public IEventListener, public EditorContext
{
public:

	// Constructor
	NOUS_EDITOR_API ModuleEditor(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, bool isGameMode,
		ModuleWindow* moduleWindow,
		ModuleInput* moduleInput,
		ModuleCamera3D* moduleCamera3D,
		ModuleResourceManager* moduleResourceManager,
		ModuleScene* moduleScene,
		ModuleRenderer3D* moduleRenderer3D);

	// Destructor
	NOUS_EDITOR_API virtual ~ModuleEditor();

	NOUS_EDITOR_API bool Awake() override;
    NOUS_EDITOR_API bool Start() override;
	NOUS_EDITOR_API bool CleanUp() override;
	NOUS_EDITOR_API void DrawEditor() override;
	NOUS_EDITOR_API void OnEvent(const Event& event) override;

    // EditorContext implementation
    ImFont*                GetFont(size_t index)        const override;
    ModuleScene*           GetScene()                   const override { return mModuleScene; }
    ModuleCamera3D*        GetCamera()                  const override { return mModuleCamera3D; }
    ModuleInput*           GetInput()                   const override { return mModuleInput; }
    ModuleResourceManager* GetResourceManager()         const override { return mModuleResourceManager; }
    RendererFrontend*      GetRendererFrontend()        const override;
    NOUS_Multithreading::NOUS_JobSystem* GetJobSystem() const override { return JobSystem; }

private:

	void InitFrame(RendererBackendType backendType);
	void InternalDrawEditor();
	void EndFrame(RendererBackendType backendType);

    IEditorWindow* GetEditorWindowByName(std::string name);

    void AddEditorWindow(IEditorWindow* editorWindow);

    // Vulkan Specific
    static VulkanContext* GetVulkanContext();

private:

	// Dependency Injection
	ModuleWindow*          mModuleWindow;
	ModuleInput*           mModuleInput;
	ModuleCamera3D*        mModuleCamera3D;
	ModuleResourceManager* mModuleResourceManager;
	ModuleScene*           mModuleScene;
	ModuleRenderer3D*      mModuleRenderer3D;

	RendererBackendType currentBackendType;

	// Custom allocator vector for editor windows
	NOUS_Vector<IEditorWindow*> editorWindows;

    // Array to store ImFont pointers
    NOUS_Vector<ImFont*> fonts;

};

#endif // MODULEEDITOR_H

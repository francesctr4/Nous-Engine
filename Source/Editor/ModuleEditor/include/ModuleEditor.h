#ifndef MODULEEDITOR_H
#define MODULEEDITOR_H

#include "Editor/EditorExport.h"

#include "Engine/Modules/Module.h"
#include <RendererFrontend/IEditorOverlay.h>
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Editor/EditorContext.h"
#include "Editor/GameExporter/include/GameExporter.h"

#include "Engine/Utils/DataStructures/NOUS_Vector.h"

#include <memory>
#include <string>

union SDL_Event;
class IEditorRenderBridge;
enum class RendererBackendType : int8_t;
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
	NOUS_EDITOR_API ModuleEditor(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
		ModuleWindow* moduleWindow,
		ModuleInput* moduleInput,
		ModuleCamera3D* moduleCamera3D,
		ModuleResourceManager* moduleResourceManager,
		ModuleScene* moduleScene,
		ModuleRenderer3D* moduleRenderer3D);

	// Destructor
	NOUS_EDITOR_API ~ModuleEditor() override;

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
    nous::engine::multithreading::NOUS_JobSystem* GetJobSystem() const override { return JobSystem; }
    GameExporter* GetGameExporter() const override { return m_gameExporter; }
    IEditorRenderBridge*   GetEditorRenderBridge()      const override { return m_renderBridge; }
    std::string GetAssetsBrowserDirectory() const override;
    void UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath) override;
    void WatchShaderFile(const std::string& path) override;

private:

	static void InitFrame(RendererBackendType backendType);
	void InternalDrawEditor();
	void EndFrame(RendererBackendType backendType) const;

    IEditorWindow* GetEditorWindowByName(const std::string& name);

    void AddEditorWindow(IEditorWindow* editorWindow);

	// --------------------------------------------------------------

	// Dependency Injection
	ModuleWindow*          mModuleWindow;
	ModuleInput*           mModuleInput;
	ModuleCamera3D*        mModuleCamera3D;
	ModuleResourceManager* mModuleResourceManager;
	ModuleScene*           mModuleScene;
	ModuleRenderer3D*      mModuleRenderer3D;

	RendererBackendType currentBackendType;

	// Resolved in Awake() from mModuleRenderer3D -- NOT in the constructor: the
	// backend is created by ModuleRenderer3D::Awake(), which runs after this
	// module is constructed in MainEditor.cpp, so a ctor parameter would be null.
	// Owned by the backend; this module only observes it.
	IEditorRenderBridge* m_renderBridge = nullptr;

    GameExporter* m_gameExporter = nullptr;

	// Custom allocator vector for editor windows
	NOUS_Vector<IEditorWindow*> editorWindows;

    // Array to store ImFont pointers
    NOUS_Vector<ImFont*> fonts;

};

#endif // MODULEEDITOR_H

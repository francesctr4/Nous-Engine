#ifndef MODULESCENE_H
#define MODULESCENE_H

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include <string>
#include <vector>
#include <mutex>
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

class Scene;
class GameObject;
class Camera;
class CScript;
class ScriptManager;

class ModuleScene : public Module, public IEventListener
{
public:

	// Constructor
	ModuleScene(Application* app);

	// Destructor
	virtual ~ModuleScene();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	NOUS_ENGINE_API void SaveScene(const std::string& path);
	NOUS_ENGINE_API void LoadScene(const std::string& path);
	NOUS_ENGINE_API void ClearScene();

	// Called by CScript::OnStart / CScript::OnDestroy to maintain the live registry
	NOUS_ENGINE_API void RegisterScriptComponent(CScript* component);
	NOUS_ENGINE_API void UnregisterScriptComponent(CScript* component);

public:

	Scene*         activeScene;
	GameObject*    selectedGameObject;
	Camera*        gameCamera;
	ScriptManager* scriptManager;

private:

	// Flat registry of every live CScript component in the scene.
	// Used for hot-reload and LateUpdate dispatch.
	NOUS_Vector<CScript*> m_scriptComponents;
	std::mutex            m_scriptComponentsMutex;

	void RecompileScripts();
	void CleanupScripts();

	/**
	 * @brief Ensures at least one GameObject with a main CCamera exists in the
	 *        active scene. If none is found, a "Main Camera" GO is created and
	 *        positioned at the current gameCamera location.
	 *
	 * Called at the end of every LoadScene job so that scenes saved before the
	 * CCamera system existed automatically get a camera on load.
	 */
	void EnsureMainCamera();
};

#endif // MODULESCENE_H
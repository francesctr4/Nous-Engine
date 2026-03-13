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
class IScript;
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

public:

	Scene* activeScene;
	GameObject* selectedGameObject;
	Camera* gameCamera;

	// Scripting
	ScriptManager* scriptManager;
	NOUS_Vector<IScript*> scripts;
	std::mutex scriptsMutex;

	void CreateScriptInstances();
	void RecompileScripts();
	void CleanupScripts();
};

#endif // MODULESCENE_H
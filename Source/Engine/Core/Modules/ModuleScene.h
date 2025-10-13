#ifndef MODULESCENE_H
#define MODULESCENE_H

#include "Engine/Core/Module.h"
#include <string>
#include <vector>

class Scene;
class GameObject;
class Camera;
class IScript;
class ScriptManager;

class ModuleScene : public Module
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

	void ReceiveEvent(const Event& event) override;

    void SaveScene(const std::string& path);
    void LoadScene(const std::string& path);
    void ClearScene();

public:

	Scene* activeScene;
	GameObject* selectedGameObject;
	Camera* gameCamera;

	// Scripting
	ScriptManager* scriptManager;
	std::vector<IScript*> scripts;

	void CreateScriptInstances();
	void RecompileScripts();
	void CleanupScripts();
};

#endif // MODULESCENE_H
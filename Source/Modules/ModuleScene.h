#ifndef MODULESCENE_H
#define MODULESCENE_H

#include "Modules/Module.h"

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

public:

	Camera* gameCamera;

	// Scripting
	ScriptManager* scriptManager;
	std::vector<IScript*> scripts;

	void CreateScriptInstances();
	void RecompileScripts();
	void CleanupScripts();
};

#endif // MODULESCENE_H
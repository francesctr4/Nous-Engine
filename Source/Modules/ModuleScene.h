#ifndef MODULESCENE_H
#define MODULESCENE_H

#include "Modules/Module.h"

class Camera;

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

};

#endif // MODULESCENE_H
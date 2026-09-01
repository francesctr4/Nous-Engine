#pragma once

#include <ModuleBase/Module.h>
#include <EngineCore/EngineExport.h>
#include <EngineCore/UpdateStatus.h>
#include <EventSystem/IEventListener.h>

class ModuleAI : public Module, public IEventListener
{
public:

	ModuleAI(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);
	~ModuleAI() override;

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

private:

};

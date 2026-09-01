#pragma once

#include <ModuleBase/Module.h>
#include <EngineCore/EngineExport.h>
#include <EngineCore/UpdateStatus.h>
#include <EventSystem/IEventListener.h>

class ModuleAnimation : public Module, public IEventListener
{
public:

	ModuleAnimation(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);
	~ModuleAnimation() override;

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

private:

};

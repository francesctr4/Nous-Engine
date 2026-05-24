#pragma once

#include "Engine/Modules/Module.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

class ModuleAudio : public Module, public IEventListener
{
public:

    // Constructor
    ModuleAudio(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);

    // Destructor
    ~ModuleAudio() override;

    bool Awake() override;
    bool Start() override;
    UpdateStatus PreUpdate(float dt) override;
    UpdateStatus Update(float dt) override;
    UpdateStatus PostUpdate(float dt) override;
    bool CleanUp() override;

    void OnEvent(const Event& event) override;

    // ----------------------------------------

private:

};

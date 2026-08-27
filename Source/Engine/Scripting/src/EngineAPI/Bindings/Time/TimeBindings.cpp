#include <Scripting/EngineAPI/Bindings/TimeBindings.h>

#include <TimeManager/TimeManager.h>

void SetupTimeBindings(TimeAPI& time)
{
    time.GetDeltaTime    = []() -> float { return TimeManager::deltaTime; };
    time.GetSimDeltaTime = []() -> float { return TimeManager::simulationDeltaTime; };
    time.GetElapsedTime  = []() -> float { return TimeManager::simulationTime; };
    time.GetFrameCount   = []() -> int   { return TimeManager::frameCount; };
    time.GetSimFrameCount= []() -> int   { return TimeManager::simulationFrameCount; };
}

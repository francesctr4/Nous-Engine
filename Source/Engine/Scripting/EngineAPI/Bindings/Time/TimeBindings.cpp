#include "Engine/Scripting/EngineAPI/Bindings/Time/TimeBindings.h"

#include "Engine/Core/TimeManager/TimeManager.h"

void SetupTimeBindings(TimeAPI& time)
{
    time.GetDeltaTime    = []() -> float { return TimeManager::deltaTime; };
    time.GetSimDeltaTime = []() -> float { return TimeManager::simulationDeltaTime; };
    time.GetElapsedTime  = []() -> float { return TimeManager::simulationTime; };
    time.GetFrameCount   = []() -> int   { return TimeManager::frameCount; };
    time.GetSimFrameCount= []() -> int   { return TimeManager::simulationFrameCount; };
}

// File: ScriptBindings.h
#ifndef NOUS_ENGINE_SCRIPTBINDINGS_H
#define NOUS_ENGINE_SCRIPTBINDINGS_H

#include "EngineAPI.h"

class ScriptBindings
{
public:
    static void SetupAllBindings(EngineAPI& api);
    static void SetupLoggerBindings(LoggerAPI& logger);
    static void SetupInputBindings(InputAPI& input);
    // Add future binding setup methods here:
    // static void SetupPhysicsBindings(PhysicsAPI& physics);
    // static void SetupInputBindings(InputAPI& input);
};

#endif // NOUS_ENGINE_SCRIPTBINDINGS_H

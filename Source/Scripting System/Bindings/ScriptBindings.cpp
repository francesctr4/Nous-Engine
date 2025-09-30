// File: LoggerBindings.cpp
#include "Scripting System/Bindings/ScriptBindings.h"
#include "Utils/Logger.h"
#include "Core/Application.h"
#include "Modules/ModuleInput.h"

void ScriptBindings::SetupAllBindings(EngineAPI& api)
{
    SetupLoggerBindings(api.Logger);
    SetupInputBindings(api.Input);
}

void ScriptBindings::SetupLoggerBindings(LoggerAPI& logger)
{
    logger.Trace  = [](const char* msg) { NOUS_TRACE("[SCRIPT] %s", msg); };
    logger.Debug  = [](const char* msg) { NOUS_DEBUG("[SCRIPT] %s", msg); };
    logger.Info   = [](const char* msg) { NOUS_INFO("[SCRIPT] %s", msg); };
    logger.Warn   = [](const char* msg) { NOUS_WARN("[SCRIPT] %s", msg); };
    logger.Error  = [](const char* msg) { NOUS_ERROR("[SCRIPT] %s", msg); };
    logger.Fatal  = [](const char* msg) { NOUS_FATAL("[SCRIPT] %s", msg); };
}

void ScriptBindings::SetupInputBindings(InputAPI &input)
{
    input.GetKey = [](int id) { External->input->GetKey(id); };
}

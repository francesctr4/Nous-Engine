// EngineAPI.cpp inside scripts DLL
#include <Scripting/EngineAPI/EngineAPI.h>

// Define the single instance for the DLL
EngineAPI* Nous_Engine = nullptr;

extern "C" NOUS_SCRIPT_EXPORT void NOUS_SCRIPT_CDECL SetEngineAPI(EngineAPI* api) {
    Nous_Engine = api;
}
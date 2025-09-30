// EngineAPI.cpp inside scripts DLL
#include "EngineAPI.h"

// Define the single instance for the DLL
EngineAPI* Nous_Engine = nullptr;

extern "C" __declspec(dllexport) void __cdecl SetEngineAPI(EngineAPI* api) {
    Nous_Engine = api;
}
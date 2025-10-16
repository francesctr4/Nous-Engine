#ifndef VULKANEXTERNAL_H
#define VULKANEXTERNAL_H

#include <Engine/Core/Globals.h>
#include <Engine/Core/EngineExport.h>

void ExecuteBatchFile(const char* batchFilePath);

// External: ModuleWindow.h
struct SDL_Window;
NOUS_ENGINE_API SDL_Window* GetSDLWindowData();
NOUS_ENGINE_API void GetFramebufferSize(int32* width, int32* height);

#endif // VULKANEXTERNAL_H
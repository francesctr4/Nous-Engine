#ifndef VULKANEXTERNAL_H
#define VULKANEXTERNAL_H

#include "Core/Globals.h"

void ExecuteBatchFile(const char* batchFilePath);

// External: ModuleWindow.h
struct SDL_Window;
SDL_Window* GetSDLWindowData();
void GetFramebufferSize(int32* width, int32* height);

#endif // VULKANEXTERNAL_H
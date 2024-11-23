#pragma once

#include "Globals.h"

void ExecuteBatchFile(const char* batchFilePath);

// External: ModuleWindow.h
struct SDL_Window;
SDL_Window* GetSDLWindowData();
void GetFramebufferSize(int32* width, int32* height);
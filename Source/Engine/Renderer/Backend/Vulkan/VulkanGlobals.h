#ifndef VULKANGLOBALS_H
#define VULKANGLOBALS_H

#include <Engine/Core/Globals.h>

// ---------------- Vulkan Validation Layers ---------------- //

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

#endif // VULKANGLOBALS_H
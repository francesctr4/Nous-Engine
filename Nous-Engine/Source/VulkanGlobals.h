#pragma once

#include "Globals.h"

// ---------------- Vulkan Validation Layers ---------------- //

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif
#pragma once

#include "Globals.h"

// --------------- Vulkan Max Frames in Flight --------------- //

const uint8 MAX_FRAMES_IN_FLIGHT = 2;

// ---------------- Vulkan Validation Layers ---------------- //

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif
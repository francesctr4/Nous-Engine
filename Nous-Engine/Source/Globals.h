#pragma once

#include <iostream>
#include <cstdint>
#include "STL.h"

#include "Logger.h"

// Threading Library
#include <thread>
#include <barrier>
#include <chrono>

#define TITLE "Nous Engine"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// ---------- Type Definitions ---------- \\

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

// -------------------------------------- \\

static int32 cachedFramebufferWidth = 0;
static int32 cachedFramebufferHeight = 0;

#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif // !_WIN64

typedef enum UpdateStatus
{
	UPDATE_CONTINUE = 1,
	UPDATE_STOP = 2,
	UPDATE_ERROR = 3

} UpdateStatus;

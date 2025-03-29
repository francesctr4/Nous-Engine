#pragma once

#include <iostream>
#include <cstdint>
#include <format>
#include "STL.h"

#include "Logger.h"
#include "Asserts.h"

constexpr const char* TITLE = "Nous Engine";
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;
constexpr float DEFAULT_TARGET_FPS = 144.00f;

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

/**
 * @brief Any id set to this should be considered invalid,
 * and not actually pointing to a real object.
 */
constexpr unsigned int INVALID_ID = 4294967295U;

// Gibibytes (2^30)
constexpr uint64 GiB(uint64 amount) 
{
    return amount * 1024ULL * 1024 * 1024;
}

// Mebibytes (2^20)
constexpr uint64 MiB(uint64 amount) 
{
    return amount * 1024ULL * 1024;
}

// Kibibytes (2^10)
constexpr uint64 KiB(uint64 amount) 
{
    return amount * 1024ULL;
}

// Gigabytes (10^9)
constexpr uint64 GB(uint64 amount) 
{
    return amount * 1000ULL * 1000 * 1000;
}

// Megabytes (10^6)
constexpr uint64 MB(uint64 amount) 
{
    return amount * 1000ULL * 1000;
}

// Kilobytes (10^3)
constexpr uint64 KB(uint64 amount) 
{
    return amount * 1000ULL;
}
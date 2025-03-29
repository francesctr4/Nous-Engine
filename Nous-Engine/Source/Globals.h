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

/** @brief Gets the number of bytes from amount of gibibytes (GiB) (1024*1024*1024) */
#define GIBIBYTES(amount) amount * 1024 * 1024 * 1024
/** @brief Gets the number of bytes from amount of mebibytes (MiB) (1024*1024) */
#define MEBIBYTES(amount) amount * 1024 * 1024
/** @brief Gets the number of bytes from amount of kibibytes (KiB) (1024) */
#define KIBIBYTES(amount) amount * 1024

/** @brief Gets the number of bytes from amount of gigabytes (GB) (1000*1000*1000) */
#define GIGABYTES(amount) amount * 1000 * 1000 * 1000
/** @brief Gets the number of bytes from amount of megabytes (MB) (1000*1000) */
#define MEGABYTES(amount) amount * 1000 * 1000
/** @brief Gets the number of bytes from amount of kilobytes (KB) (1000) */
#define KILOBYTES(amount) amount * 1000
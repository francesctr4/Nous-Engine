#pragma once

#include <iostream>
#include <cstdint>

// Threading Library
#include <thread>
#include <barrier>
#include <chrono>

#define TITLE "Nous Engine"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif // !_WIN64

typedef enum UpdateStatus
{
	UPDATE_CONTINUE = 1,
	UPDATE_STOP = 2,
	UPDATE_ERROR = 3

} UpdateStatus;
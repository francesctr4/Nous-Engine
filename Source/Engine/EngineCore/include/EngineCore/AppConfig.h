#pragma once

// Application-level configuration constants.
//
// Split out of the old Globals.h, which had ~74 includers because it mixed five
// unrelated concerns. These five are the "what application is this" set: the
// window title (also reported to Vulkan as the application/engine name), the
// default window size, and the main-loop timing defaults.

constexpr auto  TITLE                  = "Nous Engine";
constexpr int   WINDOW_WIDTH           = 800;
constexpr int   WINDOW_HEIGHT          = 600;
constexpr float DEFAULT_TARGET_FPS     = 144.00F;
constexpr float DEFAULT_SPIN_THRESHOLD = 0.002F;  // spin for the last 2ms before frame deadline

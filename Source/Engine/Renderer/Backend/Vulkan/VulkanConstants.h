#pragma once

#include <cstdint>

// ── Swapchain image-count cap ─────────────────────────────────────────────────
//
// Hard upper bound on the number of swapchain images the renderer supports.
// Every per-swapchain-image inline array (instance descriptor sets, per-binding
// generation/id tracking) is sized to this; the actual count used at runtime is
// swapChain.swapChainImages.size(), which CreateSwapChain asserts is <= this cap.
//
// Real swapchains report 2-4 images (FIFO ≥ 2, mailbox ≥ 3, Mesa llvmpipe = 4).
// 8 gives generous headroom while keeping the per-instance inline arrays small.
constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 8;

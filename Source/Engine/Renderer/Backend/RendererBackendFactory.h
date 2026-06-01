#pragma once

#include "Engine/Renderer/Backend/iRendererBackend.h"

/**
 * @brief Instantiates the concrete renderer backend for the given API.
 *
 * Replaces the old RendererBackend passthrough wrapper: RendererFrontend now
 * owns an IRendererBackend* directly and calls this factory to create it. Only
 * this translation unit includes the concrete backend headers (e.g. Vulkan),
 * so it remains the compile firewall between the frontend and the API impl.
 *
 * @return Heap-allocated backend (NOUS_NEW, MemoryTag::RENDERER) or nullptr on
 *         an unknown/unsupported type. Caller owns it (NOUS_DELETE).
 */
[[nodiscard]] IRendererBackend* CreateRendererBackend(RendererBackendType type);

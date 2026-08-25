#pragma once

#include "Engine/Core/Globals.h"

struct SDL_Window;

// -----------------------------------------------------------------------------
// The OS window, as seen from inside Renderer/.
// -----------------------------------------------------------------------------
/**
 * @brief The window surface a backend presents to.
 *
 * Implemented by ModuleWindow so that Renderer/ can create its presentation
 * surface and size its swapchain without depending on Modules/. Mirrors the
 * IResourceLoader / ISceneHost seams that keep Systems/ off Modules/.
 *
 * SDL_Window* is deliberately part of the contract: SDL is a third-party
 * dependency Renderer/ already links (VulkanInstance calls
 * SDL_Vulkan_CreateSurface directly), not an engine layer. The alternative --
 * a CreateVulkanSurface(VkInstance, ...) method here -- would drag Vulkan into
 * the module layer, which is strictly worse.
 */
class IRenderWindow
{
public:
    virtual ~IRenderWindow() = default;

    /** @brief The native window handle to create a presentation surface against. */
    [[nodiscard]] virtual SDL_Window* GetSDL_Window() const = 0;

    /**
     * @brief Drawable size in pixels, which is NOT the logical window size on
     *        HiDPI displays.
     *
     * Only a starting point for the swapchain: the surface extent the driver
     * reports wins (X11/llvmpipe clamps it a few px off). See CreateSwapChain.
     */
    virtual void GetFramebufferSize(int32* width, int32* height) const = 0;
};

/**
 * @file RendererTypes.cpp
 * @brief Translation unit for RendererTypes.h to ensure Renderer.lib is generated.
 *
 * This file intentionally contains minimal code. It exists so that the Renderer
 * static library has at least one compiled translation unit, enabling linkage
 * with Nous-Engine.dll and other modules.
 */

#include <Renderer/RendererTypes.h>

namespace nous::engine::renderer
{
    // Acts as a linkage anchor for the Renderer module.
    // You can also place explicit template instantiations here later if needed.
    void Renderer_LinkAnchor() {}
}

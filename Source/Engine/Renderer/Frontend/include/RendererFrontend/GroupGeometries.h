#pragma once

#include "Engine/EngineExport.h"
#include <Renderer/RendererTypes.h>

#include <vector>

struct GroupedGeometries
{
    std::vector<glm::mat4>      matrices;
    std::vector<InstancedBatch> batches;
};

// Groups a flat geometry list into instanced batches sorted by (material, mesh).
// baseInstance: SSBO index offset for this pass — 0 for the scene pass, c_maxInstances for game.
NOUS_ENGINE_API GroupedGeometries GroupGeometries(
    const std::vector<GeometryRenderData>& geometries,
    uint32_t baseInstance = 0);

#pragma once

#include <Renderer/RendererTypes.h>

#include <vector>

struct GroupedGeometries
{
    std::vector<glm::mat4>      matrices;
    std::vector<uint32_t>       paletteBases;  // parallel to matrices; c_noSkinPalette = unskinned
    std::vector<glm::mat4>      palettes;      // every skinned instance's palette, concatenated
    std::vector<InstancedBatch> batches;
};

// Groups a flat geometry list into instanced batches sorted by (material, mesh).
//
// baseInstance:    SSBO index offset for this pass — 0 for the scene pass, c_maxInstances for game.
// basePaletteSlot: palette-buffer offset for this pass — 0 for scene, c_maxSkinnedBones for game.
//
// Palettes are indexed PER INSTANCE via paletteBases[gl_InstanceIndex], not selected
// per batch, so two characters sharing a mesh and material still collapse into one
// instanced draw despite holding different poses.
GroupedGeometries GroupGeometries(
    const std::vector<GeometryRenderData>& geometries,
    uint32_t baseInstance    = 0,
    uint32_t basePaletteSlot = 0);

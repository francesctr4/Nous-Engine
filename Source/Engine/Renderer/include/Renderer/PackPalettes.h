#pragma once

#include <Renderer/RendererTypes.h>

#include <span>
#include <vector>

// Per-instance palette bases plus the concatenated palettes they index into.
struct PackedPalettes
{
    std::vector<uint32_t>  bases;     // parallel to the input; c_noSkinPalette = unskinned
    std::vector<glm::mat4> palettes;  // every skinned entry's palette, back to back
};

// Concatenates the bone palettes of an ALREADY-ORDERED geometry list and records
// where each one starts.
//
// Takes a pointer span rather than the geometry vector because callers disagree on
// order: GroupGeometries packs in its (material, mesh)-sorted order so bases line up
// with gl_InstanceIndex, while the per-object pick and outline passes iterate their
// lists as given. One implementation keeps the sentinel and overflow rules from
// drifting between the three.
//
// It lives in Renderer/ -- the shared-types target -- rather than beside
// GroupGeometries, because the pick and outline passes that call it are inside
// VulkanBackend. Owning it in the frontend would make the backend depend on the
// frontend, which is the wrong direction; Renderer/ already owns GeometryRenderData
// and sits below both.
//
// basePaletteSlot is the offset of this pass's region inside the shared palette
// buffer, so a returned base is a GLOBAL index while `palettes` stays pass-local --
// the backend writes the run at basePaletteSlot.
//
// A null entry, a null palette and an EMPTY palette all yield c_noSkinPalette, and
// all still consume a base slot: dropping one would shift every later base onto the
// wrong character.
PackedPalettes PackPalettes(std::span<const GeometryRenderData* const> ordered,
                            uint32_t basePaletteSlot);

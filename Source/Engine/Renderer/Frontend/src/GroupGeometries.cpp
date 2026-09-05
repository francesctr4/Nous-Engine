#include <RendererFrontend/GroupGeometries.h>
#include <Renderer/PackPalettes.h>
#include <Logger/Logger.h>

#include <algorithm>
#include <span>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_FRONTEND;

GroupedGeometries GroupGeometries(
    const std::vector<GeometryRenderData>& geometries,
    const uint32_t baseInstance,
    const uint32_t basePaletteSlot)
{
    GroupedGeometries result;
    if (geometries.empty()) return result;

    // Sort so identical (material, mesh) pairs are contiguous — required for correct firstInstance offsets.
    std::vector<const GeometryRenderData*> sorted;
    sorted.reserve(geometries.size());
    for (const auto& g : geometries) sorted.push_back(&g);
    std::sort(sorted.begin(), sorted.end(), [](const GeometryRenderData* a, const GeometryRenderData* b)
    {
        if (a->material != b->material) return a->material < b->material;
        return a->geometry < b->geometry;
    });

    // The geometries that actually became instances, in emission order. Kept
    // separately from `sorted` because entries are skipped (null mesh or material,
    // instance limit), so `sorted` and `matrices` do not line up — packing bases
    // from a prefix of `sorted` would hand an instance another object's palette.
    std::vector<const GeometryRenderData*> accepted;
    accepted.reserve(geometries.size());

    for (const GeometryRenderData* grd : sorted)
    {
        if (!grd->geometry || !grd->material) continue;
        if (result.matrices.size() >= c_maxInstances)
        {
            NOUS_WARN_C(CURRENT_CHANNEL,
                "Instance limit (%u) reached — remaining geometries in this pass will be skipped.",
                c_maxInstances);
            break;
        }

        // Local index within this pass's matrix array; add baseInstance to get the SSBO index.
        const uint32_t localIndex = static_cast<uint32_t>(result.matrices.size());
        const uint32_t ssboIndex  = baseInstance + localIndex;
        result.matrices.push_back(grd->model);
        accepted.push_back(grd);

        // Same test PackPalettes applies: a null or empty palette is "not skinned".
        const bool skinned = grd->palette && !grd->palette->empty();

        if (!result.batches.empty() &&
            result.batches.back().geometry == grd->geometry &&
            result.batches.back().material == grd->material)
        {
            result.batches.back().instanceCount++;
            result.batches.back().hasSkinnedInstances |= skinned;
        }
        else
        {
            InstancedBatch batch;
            batch.geometry            = grd->geometry;
            batch.material            = grd->material;
            batch.firstInstance       = ssboIndex;
            batch.instanceCount       = 1;
            batch.hasSkinnedInstances = skinned;
            result.batches.push_back(batch);
        }
    }

    // Bases are packed from the SORTED, accepted list, so paletteBases[i] lines up
    // with matrices[i] — and therefore with gl_InstanceIndex once firstInstance is
    // applied.
    PackedPalettes packed = PackPalettes(
        std::span<const GeometryRenderData* const>(accepted.data(), accepted.size()),
        basePaletteSlot);

    result.paletteBases = std::move(packed.bases);
    result.palettes     = std::move(packed.palettes);

    return result;
}

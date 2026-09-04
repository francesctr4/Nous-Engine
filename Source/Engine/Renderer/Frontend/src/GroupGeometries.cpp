#include <RendererFrontend/GroupGeometries.h>
#include <Logger/Logger.h>

#include <algorithm>

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

        // Palette base for this instance. A null or empty palette is the "not
        // skinned" case — and so is running out of bone budget, which degrades the
        // character to bind pose instead of reading past the buffer.
        uint32_t paletteBase = c_noSkinPalette;
        if (grd->palette && !grd->palette->empty())
        {
            const size_t boneCount = grd->palette->size();
            if (result.palettes.size() + boneCount <= c_maxSkinnedBones)
            {
                paletteBase = basePaletteSlot + static_cast<uint32_t>(result.palettes.size());
                result.palettes.insert(result.palettes.end(),
                                       grd->palette->begin(), grd->palette->end());
            }
            else
            {
                NOUS_WARN_C(CURRENT_CHANNEL,
                    "Skinned bone limit (%u) reached — remaining characters in this pass render in bind pose.",
                    c_maxSkinnedBones);
            }
        }
        result.paletteBases.push_back(paletteBase);

        if (!result.batches.empty() &&
            result.batches.back().geometry == grd->geometry &&
            result.batches.back().material == grd->material)
        {
            result.batches.back().instanceCount++;
        }
        else
        {
            InstancedBatch batch;
            batch.geometry      = grd->geometry;
            batch.material      = grd->material;
            batch.firstInstance = ssboIndex;
            batch.instanceCount = 1;
            result.batches.push_back(batch);
        }
    }

    return result;
}

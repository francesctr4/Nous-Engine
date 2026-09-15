#include <Renderer/PackPalettes.h>

#include <Logger/Logger.h>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_FRONTEND;

PackedPalettes PackPalettes(std::span<const GeometryRenderData* const> ordered,
                            const uint32_t basePaletteSlot)
{
    PackedPalettes result;
    result.bases.reserve(ordered.size());

    bool warned = false;

    for (const GeometryRenderData* grd : ordered)
    {
        uint32_t base = c_noSkinPalette;

        if (grd && grd->palette && !grd->palette->empty())
        {
            const size_t boneCount = grd->palette->size();
            if (result.palettes.size() + boneCount <= c_maxSkinnedBones)
            {
                base = basePaletteSlot + static_cast<uint32_t>(result.palettes.size());
                result.palettes.insert(result.palettes.end(),
                                       grd->palette->begin(), grd->palette->end());
            }
            else if (!warned)
            {
                // Once per pass, not once per character: a scene over the limit would
                // otherwise emit a warning per frame per character.
                warned = true;
                NOUS_WARN_C(CURRENT_CHANNEL,
                    "Skinned bone limit (%u) reached - remaining characters in this pass render in bind pose.",
                    c_maxSkinnedBones);
            }
        }

        result.bases.push_back(base);
    }

    return result;
}

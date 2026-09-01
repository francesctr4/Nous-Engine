#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace nous::engine::animation_system
{
    struct AnimClipData;
    struct AnimationBinding;

    // One playing clip. CAnimator holds two (current + next) and cross-fades them.
    //
    // Neither pointer is owned: `clip` is storage inside a ResourceAnimation and
    // `binding` is an entry in ModuleAnimation's BindingCache. Both outlive the
    // instance under the normal lifetimes -- but a resource reload invalidates
    // both, which is exactly why the cache exposes InvalidateAnimation().
    struct AnimInstance
    {
        const AnimClipData*     clip    = nullptr;
        uint32_t                clipUID = 0;
        const AnimationBinding* binding = nullptr;

        float time  = 0.0f;   // SECONDS into the clip
        float speed = 1.0f;   // negative plays backwards; the cursor handles it
        bool  loop  = true;

        // Last key index per channel, as (position, rotation, scale). Forward
        // playback then costs O(1) per channel instead of a binary search per bone
        // per frame -- the single biggest constant-factor win in the sampler,
        // because the common case advances by zero or one key.
        //
        // Parallel to clip->channels. MUST be reset whenever time jumps
        // discontinuously: on loop wrap and on any seek. FindKey() also self-heals
        // if it detects it has been left behind, so a missed reset degrades to a
        // rescan rather than to wrong output -- but do not lean on that.
        std::vector<glm::uvec3> cursor;

        // Points the instance at a clip and sizes the cursor. Call before Sample().
        void SetClip(const AnimClipData* newClip, uint32_t newClipUID,
                     const AnimationBinding* newBinding);

        void ResetCursor();

        // Jumps to an absolute time and resets the cursor. Use this rather than
        // writing `time` directly, unless you are also resetting the cursor.
        void Seek(float seconds);
    };
}

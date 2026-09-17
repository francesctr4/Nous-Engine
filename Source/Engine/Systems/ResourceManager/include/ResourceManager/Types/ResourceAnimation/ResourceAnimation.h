#pragma once

#include <AnimationSystem/Clip/AnimClip.h>
#include <AnimationSystem/Events/AnimationEvents.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <cstdint>
#include <vector>

// One animation clip.
//
// AnimClipData is composed BY VALUE for the same reason ResourceSkeleton composes
// SkeletonData: the sampler must never learn that resources exist.
//
// THE CLIP STORES NO SKELETON REFERENCE, deliberately. Binding is by bone NAME at runtime,
// which is what lets a clip extracted from an anim-only FBX drive the rig that came with
// the skinned one.

// How the clip is meant to PLAY, as authored. Not on AnimClipData: that library stays
// glm-only and about sampling, while the resource layer owns authoring metadata.
//
// Per CLIP, never per animator -- one animator routinely holds an idle that must loop and
// an attack that must not, and a flag on the component cannot say both. AnimInstance
// carries the same two fields as RUNTIME state, seeded from here every frame.
struct AnimationSettings
{
    bool  loop  = true;
    float speed = 1.0f;   // negative plays backwards; the sampler's cursor handles it
};

// Everything about a clip a HUMAN authored, as opposed to what the exporter produced.
// Bundled rather than passed separately because SaveClip's argument is deliberately
// REQUIRED -- the model import must read the existing values back or a re-import silently
// resets them -- and a fourth required parameter is the version of that which drifts.
struct ClipAuthoring
{
    AnimationSettings                                           settings;
    std::vector<nous::engine::animation_system::AnimationEvent> events;
};

// NO GPU RESIDENCY. See ImporterAnimation.
class ResourceAnimation : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAnimation(uint32_t uid);

    nous::engine::animation_system::AnimClipData clip;

    AnimationSettings settings;

    // Named markers the runtime fires as the clip plays. No generation counter: the
    // animator reads this vector directly every frame, so an editor edit is live with
    // nothing to invalidate.
    std::vector<nous::engine::animation_system::AnimationEvent> events;
};

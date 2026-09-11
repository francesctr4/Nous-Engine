#pragma once

#include <AnimationSystem/AnimClip.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <cstdint>

// One animation clip.
//
// AnimClipData is composed BY VALUE for the same reason ResourceSkeleton composes
// SkeletonData: the sampler must never learn that resources exist, or the pure
// animation library loses the independence that lets its tests link glm and gtest
// and nothing else.
//
// THE CLIP STORES NO SKELETON REFERENCE, deliberately. Binding is by bone NAME at
// runtime, which is what lets a clip extracted from an anim-only Mixamo FBX drive
// the rig that came with the skinned one -- Unity's "Copy From Other Avatar",
// expressed in engine terms.
//
// How the clip is meant to PLAY, as authored. Deliberately not on AnimClipData:
// AnimationSystem stays glm-only and about sampling, while the resource layer owns
// authoring metadata -- the same split that already has ResourceAnimation compose
// AnimClipData by value rather than inherit from it.
//
// These are per CLIP, never per animator. One animator routinely holds an idle that
// must loop and an attack that must not, and a flag on the component cannot say
// both. AnimInstance carries the same two fields as RUNTIME state; RebindTrack and
// the per-frame push in CAnimator::OnUpdate seed those from here.
struct AnimationSettings
{
    bool  loop  = true;
    float speed = 1.0f;   // negative plays backwards; the sampler's cursor handles it
};

// NO GPU RESIDENCY. See ImporterAnimation.
class ResourceAnimation : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAnimation(uint32_t uid);

    nous::engine::animation_system::AnimClipData clip;

    AnimationSettings settings;
};

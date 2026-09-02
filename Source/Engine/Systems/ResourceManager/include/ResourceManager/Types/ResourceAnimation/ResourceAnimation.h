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
// NO GPU RESIDENCY. See ImporterAnimation.
class ResourceAnimation : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAnimation(uint32_t uid);

    nous::engine::animation_system::AnimClipData clip;
};

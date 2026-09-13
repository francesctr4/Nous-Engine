#pragma once

#include <AnimationSystem/Controller.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

class ResourceAnimation;

/**
 * @brief What a state's clip slot says on disk, before anything resolves it.
 *
 * Kept alongside the resolved pointer rather than replaced by it, because a slot
 * whose asset is MISSING still has to survive a save: resolving fails, `clips`
 * gets a null, and if the write path read only the resolved pointer the binding
 * would be silently erased from the asset the next time anyone touched the graph.
 * A missing clip must stay a broken reference the user can fix, not a blank one.
 *
 * The resource-layer half of a state, so it lives here and not on ControllerState
 * -- the pure layer carries no paths and no uids.
 */
struct ControllerClipSlot
{
    std::string assetPath;
    std::string libraryPath;
    uint32_t    uid = 0;
};

/**
 * @brief An authored animation state machine.
 *
 * CPU-only, like ResourceAudioGraph: Upload/Release do no GPU work, because a
 * controller is a decision table and nothing about it is resident on a device.
 *
 * ControllerGraph is composed BY VALUE, the same split ResourceSkeleton and
 * ResourceAnimation already use -- the pure evaluator must never learn resources
 * exist, or AnimationSystem loses the property that lets its tests link glm alone.
 */
class ResourceAnimationController : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAnimationController(uint32_t uid = 0);
    NOUS_ENGINE_API ~ResourceAnimationController() override;

    nous::engine::animation_system::ControllerGraph graph;

    // PARALLEL to graph.states and indexed by ControllerState::clipIndex -- NOT by
    // state index. The two happen to coincide today because Deserialize fills one
    // clip slot per state, but clipIndex is what the pure layer carries and what
    // CAnimator::ClipForState reads, so an index derived any other way is a bug
    // waiting for the first state that resolves no clip.
    //
    // An entry may be null (a state whose clip is unassigned or failed to resolve);
    // the runtime treats that state as unplayable rather than as an error.
    //
    // NON-OWNING in the C++ sense but reference-COUNTED: Deserialize acquires each
    // one and Evict releases them. See the hazard note on ImporterAnimationController.
    std::vector<ResourceAnimation*> clips;

    // What the asset AUTHORED for each state's clip, parallel to graph.states and
    // indexed by state index (not clipIndex -- this one exists before anything has
    // resolved). The write path reads these, so a slot pointing at a missing asset
    // round-trips instead of being blanked.
    std::vector<ControllerClipSlot> clipSlots;

    // Node positions, parallel to graph.states. Opaque editor view-state, ignored by
    // the runtime -- ResourceAudioGraph::editorPositions verbatim. It is HERE rather
    // than on ControllerState so the pure layer stays free of view-state.
    std::vector<glm::vec2> editorPositions;

    // Bumped on every editor save. CAnimator compares it each frame and rebuilds
    // when it changes, which is what makes tuning a transition reach a character
    // that is already playing. Same mechanism as ResourceAudioGraph.
    uint32_t generation = 0;
};

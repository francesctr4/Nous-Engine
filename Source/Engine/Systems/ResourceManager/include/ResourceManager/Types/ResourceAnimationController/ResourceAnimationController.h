#pragma once

#include <AnimationSystem/Controller/Controller.h>
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
 * Kept alongside the resolved pointer rather than replaced by it: a slot whose asset is
 * MISSING still has to survive a save, and if the write path read only the resolved
 * pointer the binding would be silently erased the next time anyone touched the graph. A
 * missing clip must stay a broken reference the user can fix, not a blank one.
 *
 * The resource-layer half of a state -- the pure layer carries no paths and no uids.
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
 * CPU-only, like ResourceAudioGraph: Upload/Release do no GPU work, because a controller
 * is a decision table and nothing about it is resident on a device.
 *
 * ControllerGraph is composed by value, the same split ResourceSkeleton and
 * ResourceAnimation use -- the pure evaluator must never learn resources exist.
 */
class ResourceAnimationController : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAnimationController(uint32_t uid = 0);
    NOUS_ENGINE_API ~ResourceAnimationController() override;

    nous::engine::animation_system::ControllerGraph graph;

    // Indexed by ControllerState::clipIndex -- NOT by state index. The two coincide today
    // only because Deserialize fills one slot per state, and clipIndex is what the pure
    // layer carries, so an index derived any other way breaks on the first state that
    // resolves no clip.
    //
    // An entry may be null (unassigned, or failed to resolve); the runtime treats that
    // state as unplayable rather than as an error. Non-owning in the C++ sense but
    // reference-COUNTED: Deserialize acquires each one and Evict releases them.
    std::vector<ResourceAnimation*> clips;

    // What the asset AUTHORED for each state's clip, parallel to graph.states and indexed
    // by STATE index -- this one exists before anything has resolved. The write path reads
    // these, so a slot pointing at a missing asset round-trips instead of being blanked.
    std::vector<ControllerClipSlot> clipSlots;

    // Node positions, parallel to graph.states. Opaque editor view-state, ignored by the
    // runtime; here rather than on ControllerState so the pure layer stays free of it.
    std::vector<glm::vec2> editorPositions;

    // Bumped whenever THE GRAPH IS REBUILT: by the editor's save, and by Deserialize, which
    // the asset hot-reload path re-runs on a live controller when the .nctrl changes on
    // disk. CAnimator compares it each frame and re-enters its current state BY NAME, which
    // is what makes tuning a transition reach a character already playing -- and what stops
    // a reordered states array silently moving that character to another animation.
    //
    // Both writers are required: the editor edits the in-memory graph without going through
    // Deserialize, and Deserialize runs for edits the editor never saw.
    uint32_t generation = 0;
};

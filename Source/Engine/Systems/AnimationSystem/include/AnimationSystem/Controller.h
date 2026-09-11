#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nous::engine::animation_system
{
    class AnimParameters;

    /**
     * @brief How a condition tests its parameter.
     *
     * Deliberately small. Float gets only Greater/Less -- equality on a float a
     * script writes every frame is a condition that never fires, so offering it
     * would be offering a bug. Bool gets IsTrue/IsFalse rather than a value field,
     * so an editor combo cannot produce "isGrounded == 0.5".
     *
     * TriggerSet carries no value: a trigger is set or it is not, and firing
     * CONSUMES it (see EvaluateController).
     */
    enum class ConditionComparator : uint8_t
    {
        Greater,
        Less,
        IsTrue,
        IsFalse,
        TriggerSet,
    };

    struct ControllerCondition
    {
        std::string         parameter;
        ConditionComparator comparator = ConditionComparator::Greater;
        float               value      = 0.0f;   // meaningful for Greater / Less only
    };

    /**
     * @brief One edge of the graph.
     *
     * `fromState` is an index into ControllerGraph::states, or c_anyState for a
     * transition evaluated from EVERY state. Storing Any State as a sentinel index
     * rather than a separate collection is what lets the evaluator walk one
     * container and keeps authored ORDER meaningful across both kinds.
     */
    struct ControllerTransition
    {
        int   fromState   = -1;
        int   toState     = -1;
        bool  hasExitTime = false;
        float exitTime    = 0.8f;    // normalized progress through the SOURCE state
        float duration    = 0.2f;    // cross-fade seconds; <= 0 snaps

        std::vector<ControllerCondition> conditions;   // ANDed
    };

    struct ControllerState
    {
        std::string name;

        // Index into ResourceAnimationController's parallel clip array, or -1 for a
        // state with no clip assigned. The pure layer never learns resources exist
        // -- the same split that has AnimClipData carry no ResourceAnimation.
        int clipIndex = -1;

        // MULTIPLIES the clip's authored speed, never replaces it. An override would
        // recreate exactly what killed CAnimator::speed: a field on a shared asset
        // competing with the clip's own authored value, so two things claim to be
        // "the speed" and the loser is whichever ran last.
        float speed = 1.0f;

        // Optional declared Float parameter that further scales this state's rate.
        // Empty means factor 1. This is the cheap stand-in for the blend tree that
        // is deliberately out of scope: "run plays faster as input grows" is
        // otherwise unreachable, and it costs one string plus one multiply.
        std::string speedParameter;

        // Per-state root motion, as the raw enum VALUE of RootMotionMode. The pure
        // layer must not include the ECS header that declares that enum, and giving
        // it a parallel enum would be two things to keep in sync -- so it travels as
        // an int the resource layer and CAnimator agree on. 0 == Inherit, matching
        // RootMotionMode::Inherit being the first enumerator.
        int rootMotion = 0;
    };

    struct ParameterDecl
    {
        std::string name;
        uint8_t     type         = 0;      // AnimParamType value
        float       defaultValue = 0.0f;
    };

    /**
     * @brief The whole authored state machine, free of engine types.
     *
     * Composed BY VALUE by ResourceAnimationController, exactly as ResourceSkeleton
     * composes SkeletonData. That is what lets t_AnimationSystem_Controller link
     * this archive plus gtest and nothing else.
     */
    struct ControllerGraph
    {
        std::vector<ControllerState>      states;
        std::vector<ControllerTransition> transitions;
        std::vector<ParameterDecl>        parameters;

        int defaultState = -1;

        // Sentinel `fromState` marking a transition evaluated from every state.
        static constexpr int c_anyState = -2;

        // Index of the state with this exact name, or -1. Case-sensitive, and a miss
        // is -1 rather than a plausible index -- CrossFade returns false on it and the
        // editor reports it, instead of silently entering some other state.
        [[nodiscard]] int FindState(std::string_view name) const;

        [[nodiscard]] bool IsValidState(const int index) const
        { return index >= 0 && static_cast<size_t>(index) < states.size(); }
    };

    /**
     * @brief Does every condition on `transition` hold right now?
     *
     * PURE: never consumes a trigger. An empty condition list is satisfied, which
     * is what makes an unconditional transition fire on entry.
     *
     * A condition naming an undeclared parameter reads through AnimParameters'
     * fallback (0 / false / not-set), so Greater, Less, IsTrue and TriggerSet are
     * simply never satisfied -- but IsFalse IS. That asymmetry is why the editor
     * offers a dropdown over declared names rather than a text field: a mistyped
     * name in an IsFalse condition produces a transition that always fires.
     */
    [[nodiscard]] bool ConditionsSatisfied(const ControllerGraph&      graph,
                                           const ControllerTransition& transition,
                                           const AnimParameters&       params);
}

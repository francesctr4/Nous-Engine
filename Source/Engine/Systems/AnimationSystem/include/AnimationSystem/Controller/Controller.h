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
     * Deliberately small. Float gets only Greater/Less -- equality on a float a script
     * writes every frame is a condition that never fires. Bool gets IsTrue/IsFalse rather
     * than a value field, so an editor combo cannot produce "isGrounded == 0.5".
     * TriggerSet carries no value, and firing CONSUMES it (see EvaluateController).
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
     * `fromState` is an index into ControllerGraph::states, or c_anyState. Any State as a
     * sentinel index rather than a separate collection is what lets the evaluator walk one
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

        // Index into ResourceAnimationController's parallel clip array, or -1 for a state
        // with no clip. The pure layer never learns resources exist.
        int clipIndex = -1;

        // MULTIPLIES the clip's authored speed, never replaces it: an override would put a
        // field on a shared asset in competition with the clip's own value, so two things
        // would claim to be "the speed" and the loser is whichever ran last.
        float speed = 1.0f;

        // Optional declared Float parameter further scaling this state's rate; empty means
        // factor 1. The cheap stand-in for a blend tree -- "run plays faster as input
        // grows" is otherwise unreachable, and it costs one string and one multiply.
        std::string speedParameter;

        // Per-state root motion, as the raw enum VALUE of RootMotionMode: the pure layer
        // must not include the ECS header that declares it, and a parallel enum would be
        // two things to keep in sync. 0 == Inherit.
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
     * Composed by value by ResourceAnimationController, as ResourceSkeleton composes
     * SkeletonData -- which is what lets the tests link this archive plus gtest and
     * nothing else.
     */
    struct ControllerGraph
    {
        std::vector<ControllerState>      states;
        std::vector<ControllerTransition> transitions;
        std::vector<ParameterDecl>        parameters;

        int defaultState = -1;

        // Sentinel `fromState` marking a transition evaluated from every state.
        static constexpr int c_anyState = -2;

        // Index of the state with this exact name, or -1. Case-sensitive, and a miss is -1
        // rather than a plausible index, so a caller reports it instead of silently
        // entering some other state.
        [[nodiscard]] int FindState(std::string_view name) const;

        [[nodiscard]] bool IsValidState(const int index) const
        { return index >= 0 && static_cast<size_t>(index) < states.size(); }
    };

    /**
     * @brief Does every condition on `transition` hold right now?
     *
     * PURE: never consumes a trigger. An empty condition list is satisfied, which is what
     * makes an unconditional transition fire on entry.
     *
     * A condition naming an undeclared parameter reads through AnimParameters' fallback,
     * so Greater, Less, IsTrue and TriggerSet are never satisfied -- but IsFalse IS. That
     * asymmetry is why the editor offers a dropdown over declared names rather than a text
     * field: a mistyped name in an IsFalse condition always fires.
     */
    [[nodiscard]] bool ConditionsSatisfied(const ControllerTransition& transition,
                                           const AnimParameters&       params);

    struct TransitionResult
    {
        bool  fired    = false;
        int   toState  = -1;
        float duration = 0.0f;
    };

    /**
     * @brief Decide whether to leave `currentState` this frame, and for where.
     *
     * `normalizedTime` is 0..1 through the current state's clip -- during a fade the
     * INCOMING one, since the destination becomes current the instant a transition starts.
     *
     * Order: Any State transitions first, then the current state's own, both in authored
     * order. FIRST SATISFIED WINS and evaluation stops, so exactly one transition fires
     * and at most one trigger is consumed per frame.
     *
     * `params` is non-const because firing consumes the triggers the winning transition
     * matched; a transition merely evaluated -- including one blocked on exit time --
     * consumes nothing. Advances no time, touches no pose, names no resource.
     */
    [[nodiscard]] TransitionResult EvaluateController(const ControllerGraph& graph,
                                                      int                    currentState,
                                                      float                  normalizedTime,
                                                      AnimParameters&        params);
}

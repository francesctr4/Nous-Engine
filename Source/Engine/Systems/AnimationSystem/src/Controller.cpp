#include <AnimationSystem/Controller.h>

#include <AnimationSystem/AnimParameters.h>

namespace
{
    using namespace nous::engine::animation_system;

    // Eats the triggers this transition matched. Called ONLY once a transition has
    // been chosen -- which is the whole reason ConditionsSatisfied is pure. A
    // trigger named by three transitions must not be spent by whichever was checked
    // first, and one blocked on exit time must not spend it either.
    void ConsumeMatchedTriggers(const ControllerTransition& transition,
                                AnimParameters&             params)
    {
        for (const ControllerCondition& c : transition.conditions)
            if (c.comparator == ConditionComparator::TriggerSet)
                (void)params.ConsumeTrigger(c.parameter);
    }

    bool ExitTimeReached(const ControllerTransition& transition, const float normalizedTime)
    {
        if (!transition.hasExitTime)
            return true;   // no threshold means nothing to wait for

        return normalizedTime >= transition.exitTime;
    }
}

namespace nous::engine::animation_system
{
    int ControllerGraph::FindState(const std::string_view name) const
    {
        for (size_t i = 0; i < states.size(); ++i)
            if (states[i].name == name)
                return static_cast<int>(i);

        return -1;
    }

    bool ConditionsSatisfied(const ControllerGraph&      graph,
                             const ControllerTransition& transition,
                             const AnimParameters&       params)
    {
        (void)graph;   // reserved for declaration-aware validation; see design §2

        for (const ControllerCondition& c : transition.conditions)
        {
            bool ok = false;

            switch (c.comparator)
            {
                case ConditionComparator::Greater:
                    ok = params.GetFloat(c.parameter, 0.0f) > c.value;
                    break;
                case ConditionComparator::Less:
                    ok = params.GetFloat(c.parameter, 0.0f) < c.value;
                    break;
                case ConditionComparator::IsTrue:
                    ok = params.GetBool(c.parameter, false);
                    break;
                case ConditionComparator::IsFalse:
                    ok = !params.GetBool(c.parameter, false);
                    break;
                case ConditionComparator::TriggerSet:
                    // Queried, never consumed -- consumption belongs to the
                    // transition that actually fires.
                    ok = params.IsTriggerSet(c.parameter);
                    break;
            }

            if (!ok)
                return false;   // AND: one failure is enough
        }

        return true;   // an empty list is satisfied
    }

    TransitionResult EvaluateController(const ControllerGraph& graph,
                                        const int              currentState,
                                        const float            normalizedTime,
                                        AnimParameters&        params)
    {
        if (!graph.IsValidState(currentState))
            return {};

        // TWO PASSES, and the order is the design: Any State first, so "attack
        // interrupts everything" is true by construction rather than by whichever
        // link the user drew first. Within each pass, authored order decides.
        for (const int pass : { ControllerGraph::c_anyState, currentState })
        {
            for (const ControllerTransition& t : graph.transitions)
            {
                if (t.fromState != pass)
                    continue;

                // An Any State transition never re-enters the state already
                // current. Not a flag: re-entry is almost never wanted, and
                // CrossFade() expresses an explicit restart. Note this is a SKIP,
                // not a stop -- the pass must continue past it.
                if (pass == ControllerGraph::c_anyState && t.toState == currentState)
                    continue;

                // A transition into a deleted state is skipped rather than followed:
                // the editor can leave one behind, and an out-of-range index here
                // would index the state array in CAnimator.
                if (!graph.IsValidState(t.toState))
                    continue;

                if (!ExitTimeReached(t, normalizedTime))
                    continue;

                if (!ConditionsSatisfied(graph, t, params))
                    continue;

                // FIRST SATISFIED WINS: consume and return, so exactly one
                // transition fires and at most one trigger is spent per frame.
                ConsumeMatchedTriggers(t, params);
                return { true, t.toState, t.duration };
            }
        }

        return {};
    }
}

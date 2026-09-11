#include <AnimationSystem/Controller.h>

#include <AnimationSystem/AnimParameters.h>

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
}

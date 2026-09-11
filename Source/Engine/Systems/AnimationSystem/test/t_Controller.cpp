#include <AnimationSystem/Controller.h>
#include <AnimationSystem/AnimParameters.h>

#include <gtest/gtest.h>

#include <string>

namespace anim = nous::engine::animation_system;

namespace
{
    // A graph with two states and one transition between them. Tests mutate the
    // returned graph rather than each building their own, so a change to the shape
    // of ControllerGraph lands in one place.
    anim::ControllerGraph TwoStateGraph()
    {
        anim::ControllerGraph g;

        anim::ControllerState idle;
        idle.name = "Idle";
        idle.clipIndex = 0;
        g.states.push_back(idle);

        anim::ControllerState run;
        run.name = "Run";
        run.clipIndex = 1;
        g.states.push_back(run);

        anim::ControllerTransition t;
        t.fromState = 0;
        t.toState = 1;
        t.duration = 0.2f;
        g.transitions.push_back(t);

        g.defaultState = 0;
        return g;
    }

    anim::ControllerCondition FloatGreater(const std::string& name, const float value)
    {
        anim::ControllerCondition c;
        c.parameter = name;
        c.comparator = anim::ConditionComparator::Greater;
        c.value = value;
        return c;
    }
}

TEST(Controller, FloatGreaterIsSatisfiedOnlyAboveTheThreshold)
{
    anim::ControllerGraph g = TwoStateGraph();
    g.transitions[0].conditions.push_back(FloatGreater("speed", 0.1f));

    anim::AnimParameters params;

    params.SetFloat("speed", 0.0f);
    EXPECT_FALSE(anim::ConditionsSatisfied(g, g.transitions[0], params));

    params.SetFloat("speed", 0.5f);
    EXPECT_TRUE(anim::ConditionsSatisfied(g, g.transitions[0], params));
}

TEST(Controller, MultipleConditionsAreAnded)
{
    anim::ControllerGraph g = TwoStateGraph();
    g.transitions[0].conditions.push_back(FloatGreater("speed", 0.1f));

    anim::ControllerCondition grounded;
    grounded.parameter = "isGrounded";
    grounded.comparator = anim::ConditionComparator::IsTrue;
    g.transitions[0].conditions.push_back(grounded);

    anim::AnimParameters params;
    params.SetFloat("speed", 0.5f);
    params.SetBool("isGrounded", false);

    // One of two satisfied is not enough -- AND, never OR. OR is authored as a
    // second transition between the same pair.
    EXPECT_FALSE(anim::ConditionsSatisfied(g, g.transitions[0], params));

    params.SetBool("isGrounded", true);
    EXPECT_TRUE(anim::ConditionsSatisfied(g, g.transitions[0], params));
}

TEST(Controller, ATransitionWithNoConditionsIsAlwaysSatisfied)
{
    const anim::ControllerGraph g = TwoStateGraph();
    const anim::AnimParameters params;

    EXPECT_TRUE(anim::ConditionsSatisfied(g, g.transitions[0], params));
}

TEST(Controller, ConditionsSatisfiedDoesNotConsumeATrigger)
{
    anim::ControllerGraph g = TwoStateGraph();

    anim::ControllerCondition attack;
    attack.parameter = "attack";
    attack.comparator = anim::ConditionComparator::TriggerSet;
    g.transitions[0].conditions.push_back(attack);

    anim::AnimParameters params;
    params.SetTrigger("attack");

    EXPECT_TRUE(anim::ConditionsSatisfied(g, g.transitions[0], params));

    // Still set: consumption happens when a transition FIRES (Task 2), not when a
    // predicate is merely evaluated. A trigger named by three transitions must not
    // be eaten by whichever is checked first.
    EXPECT_TRUE(params.IsTriggerSet("attack"));
    EXPECT_TRUE(anim::ConditionsSatisfied(g, g.transitions[0], params));
}

TEST(Controller, IsFalseIsSatisfiedByAnAbsentParameter)
{
    // A condition naming a parameter nothing has set reads through AnimParameters'
    // fallback. For IsFalse that means SATISFIED -- deliberate, and the reason the
    // editor offers a dropdown over declared names rather than a text field: a
    // mistyped name here produces a transition that fires, not one that never does.
    anim::ControllerGraph g = TwoStateGraph();

    anim::ControllerCondition notGrounded;
    notGrounded.parameter = "isGrounded";
    notGrounded.comparator = anim::ConditionComparator::IsFalse;
    g.transitions[0].conditions.push_back(notGrounded);

    anim::AnimParameters params;
    EXPECT_TRUE(anim::ConditionsSatisfied(g, g.transitions[0], params));

    params.SetBool("isGrounded", true);
    EXPECT_FALSE(anim::ConditionsSatisfied(g, g.transitions[0], params));
}

TEST(Controller, FindStateMatchesByNameAndReportsMissesAsMinusOne)
{
    const anim::ControllerGraph g = TwoStateGraph();

    EXPECT_EQ(0, g.FindState("Idle"));
    EXPECT_EQ(1, g.FindState("Run"));

    // Case-sensitive, and a miss is -1 rather than a plausible index: CrossFade
    // returns false on this, and the editor reports it.
    EXPECT_EQ(-1, g.FindState("run"));
    EXPECT_EQ(-1, g.FindState(""));

    EXPECT_TRUE(g.IsValidState(0));
    EXPECT_FALSE(g.IsValidState(-1));
    EXPECT_FALSE(g.IsValidState(2));
    EXPECT_FALSE(g.IsValidState(anim::ControllerGraph::c_anyState));
}

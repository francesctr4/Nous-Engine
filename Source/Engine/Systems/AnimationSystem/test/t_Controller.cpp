#include <AnimationSystem/Controller.h>
#include <AnimationSystem/AnimParameters.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

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

// ---------------------------------------------------------------------------
// EvaluateController -- the firing rule
// ---------------------------------------------------------------------------

TEST(Controller, AnUnconditionalTransitionFiresImmediately)
{
    anim::ControllerGraph g = TwoStateGraph();   // no conditions, no exit time
    anim::AnimParameters params;

    const anim::TransitionResult r = anim::EvaluateController(g, 0, 0.0f, params);

    EXPECT_TRUE(r.fired);
    EXPECT_EQ(1, r.toState);
    EXPECT_FLOAT_EQ(0.2f, r.duration);
}

TEST(Controller, ExitTimeAloneFiresOnlyPastTheThreshold)
{
    anim::ControllerGraph g = TwoStateGraph();
    g.transitions[0].hasExitTime = true;
    g.transitions[0].exitTime = 0.8f;

    anim::AnimParameters params;

    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.50f, params).fired);
    EXPECT_TRUE (anim::EvaluateController(g, 0, 0.80f, params).fired);
    EXPECT_TRUE (anim::EvaluateController(g, 0, 0.95f, params).fired);
}

TEST(Controller, ExitTimeAndConditionsMustBothHold)
{
    anim::ControllerGraph g = TwoStateGraph();
    g.transitions[0].hasExitTime = true;
    g.transitions[0].exitTime = 0.8f;
    g.transitions[0].conditions.push_back(FloatGreater("speed", 0.1f));

    anim::AnimParameters params;
    params.SetFloat("speed", 0.0f);

    // Past the threshold but the condition is false.
    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.9f, params).fired);

    params.SetFloat("speed", 0.5f);

    // Condition true but not yet past the threshold.
    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.5f, params).fired);

    EXPECT_TRUE(anim::EvaluateController(g, 0, 0.9f, params).fired);
}

TEST(Controller, FiringConsumesTheTriggerItMatched)
{
    anim::ControllerGraph g = TwoStateGraph();

    anim::ControllerCondition attack;
    attack.parameter = "attack";
    attack.comparator = anim::ConditionComparator::TriggerSet;
    g.transitions[0].conditions.push_back(attack);

    anim::AnimParameters params;
    params.SetTrigger("attack");

    EXPECT_TRUE(anim::EvaluateController(g, 0, 0.0f, params).fired);
    EXPECT_FALSE(params.IsTriggerSet("attack"));

    // And so the same transition does not fire again on the next frame.
    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.0f, params).fired);
}

TEST(Controller, ATransitionThatDoesNotFireLeavesItsTriggerSet)
{
    anim::ControllerGraph g = TwoStateGraph();

    // Two conditions: the trigger is set, the float is not satisfied, so this
    // transition must NOT fire and must NOT eat the trigger.
    anim::ControllerCondition attack;
    attack.parameter = "attack";
    attack.comparator = anim::ConditionComparator::TriggerSet;
    g.transitions[0].conditions.push_back(attack);
    g.transitions[0].conditions.push_back(FloatGreater("speed", 10.0f));

    anim::AnimParameters params;
    params.SetTrigger("attack");
    params.SetFloat("speed", 0.0f);

    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.0f, params).fired);
    EXPECT_TRUE(params.IsTriggerSet("attack"));
}

TEST(Controller, ExitTimeBlockingATransitionDoesNotEatItsTrigger)
{
    // The exit-time gate runs BEFORE the conditions, but the trigger must survive
    // either ordering. A trigger eaten by a transition that was blocked on timing
    // would be a press the player made and the game silently dropped.
    anim::ControllerGraph g = TwoStateGraph();
    g.transitions[0].hasExitTime = true;
    g.transitions[0].exitTime = 0.8f;

    anim::ControllerCondition attack;
    attack.parameter = "attack";
    attack.comparator = anim::ConditionComparator::TriggerSet;
    g.transitions[0].conditions.push_back(attack);

    anim::AnimParameters params;
    params.SetTrigger("attack");

    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.1f, params).fired);
    EXPECT_TRUE(params.IsTriggerSet("attack"));

    // Still there to be spent once the threshold is reached.
    EXPECT_TRUE(anim::EvaluateController(g, 0, 0.9f, params).fired);
    EXPECT_FALSE(params.IsTriggerSet("attack"));
}

TEST(Controller, FirstSatisfiedWinsAndReorderingChangesTheWinner)
{
    anim::ControllerGraph g = TwoStateGraph();

    anim::ControllerState attack;
    attack.name = "Attack";
    attack.clipIndex = 2;
    g.states.push_back(attack);   // index 2

    anim::ControllerTransition toAttack;
    toAttack.fromState = 0;
    toAttack.toState = 2;
    g.transitions.push_back(toAttack);   // both unconditional, Run listed first

    anim::AnimParameters params;
    EXPECT_EQ(1, anim::EvaluateController(g, 0, 0.0f, params).toState);

    std::swap(g.transitions[0], g.transitions[1]);
    EXPECT_EQ(2, anim::EvaluateController(g, 0, 0.0f, params).toState);
}

TEST(Controller, OnlyOneTriggerIsConsumedPerFrame)
{
    // Two satisfied transitions, each on its own trigger. Evaluation STOPS at the
    // first fire, so the second trigger is still pending for the next frame --
    // which is what keeps a queued input from being silently swallowed by a
    // transition that never ran.
    anim::ControllerGraph g = TwoStateGraph();

    anim::ControllerState attack;
    attack.name = "Attack";
    attack.clipIndex = 2;
    g.states.push_back(attack);

    anim::ControllerCondition first;
    first.parameter = "toRun";
    first.comparator = anim::ConditionComparator::TriggerSet;
    g.transitions[0].conditions.push_back(first);

    anim::ControllerTransition toAttack;
    toAttack.fromState = 0;
    toAttack.toState = 2;
    anim::ControllerCondition second;
    second.parameter = "toAttack";
    second.comparator = anim::ConditionComparator::TriggerSet;
    toAttack.conditions.push_back(second);
    g.transitions.push_back(toAttack);

    anim::AnimParameters params;
    params.SetTrigger("toRun");
    params.SetTrigger("toAttack");

    EXPECT_EQ(1, anim::EvaluateController(g, 0, 0.0f, params).toState);
    EXPECT_FALSE(params.IsTriggerSet("toRun"));
    EXPECT_TRUE(params.IsTriggerSet("toAttack"));
}

TEST(Controller, TransitionsFromOtherStatesAreIgnored)
{
    anim::ControllerGraph g = TwoStateGraph();   // the only transition is 0 -> 1

    anim::AnimParameters params;

    // Current state is 1; nothing leaves it.
    EXPECT_FALSE(anim::EvaluateController(g, 1, 0.0f, params).fired);
}

TEST(Controller, AnInvalidCurrentStateOrTargetFiresNothing)
{
    anim::ControllerGraph g = TwoStateGraph();
    anim::AnimParameters params;

    EXPECT_FALSE(anim::EvaluateController(g, -1, 0.0f, params).fired);
    EXPECT_FALSE(anim::EvaluateController(g, 99, 0.0f, params).fired);

    // A transition pointing at a state that was deleted must be skipped, not
    // followed into an out-of-range index.
    g.transitions[0].toState = 47;
    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.0f, params).fired);
}

TEST(Controller, AnEmptyGraphFiresNothing)
{
    const anim::ControllerGraph g;
    anim::AnimParameters params;

    EXPECT_FALSE(anim::EvaluateController(g, 0, 0.0f, params).fired);
}

// ---------------------------------------------------------------------------
// Any State
// ---------------------------------------------------------------------------

namespace
{
    // Idle(0) -> Run(1) unconditional, plus Attack(2) reachable from Any State on
    // a trigger. This is the demo's shape in miniature.
    anim::ControllerGraph GraphWithAnyState()
    {
        anim::ControllerGraph g = TwoStateGraph();

        anim::ControllerState attack;
        attack.name = "Attack";
        attack.clipIndex = 2;
        g.states.push_back(attack);

        anim::ControllerTransition anyToAttack;
        anyToAttack.fromState = anim::ControllerGraph::c_anyState;
        anyToAttack.toState = 2;
        anyToAttack.duration = 0.1f;

        anim::ControllerCondition trigger;
        trigger.parameter = "attack";
        trigger.comparator = anim::ConditionComparator::TriggerSet;
        anyToAttack.conditions.push_back(trigger);

        g.transitions.push_back(anyToAttack);
        return g;
    }
}

TEST(Controller, AnyStateFiresFromAnySourceState)
{
    anim::ControllerGraph g = GraphWithAnyState();

    for (const int from : { 0, 1 })
    {
        anim::AnimParameters params;
        params.SetTrigger("attack");

        const anim::TransitionResult r = anim::EvaluateController(g, from, 0.0f, params);

        EXPECT_TRUE(r.fired) << "from state " << from;
        EXPECT_EQ(2, r.toState) << "from state " << from;
        EXPECT_FLOAT_EQ(0.1f, r.duration);
    }
}

TEST(Controller, AnyStateBeatsTheCurrentStatesOwnTransitions)
{
    anim::ControllerGraph g = GraphWithAnyState();

    // Idle -> Run is unconditional and is listed FIRST in the vector. Any State
    // must still win, or "attack interrupts everything" would depend on the order
    // the editor happened to create links in.
    anim::AnimParameters params;
    params.SetTrigger("attack");

    EXPECT_EQ(2, anim::EvaluateController(g, 0, 0.0f, params).toState);
}

TEST(Controller, AnyStateDoesNotReEnterTheCurrentState)
{
    anim::ControllerGraph g = GraphWithAnyState();

    anim::AnimParameters params;
    params.SetTrigger("attack");

    // Already in Attack. Re-entering would restart the clip every frame the
    // trigger is set -- the classic "attack stutters while the button is held".
    EXPECT_FALSE(anim::EvaluateController(g, 2, 0.0f, params).fired);

    // And the trigger is untouched, because nothing fired.
    EXPECT_TRUE(params.IsTriggerSet("attack"));
}

TEST(Controller, AnyStateOrderIsHonouredAmongItsOwnTransitions)
{
    anim::ControllerGraph g = GraphWithAnyState();

    anim::ControllerTransition anyToRun;
    anyToRun.fromState = anim::ControllerGraph::c_anyState;
    anyToRun.toState = 1;
    g.transitions.push_back(anyToRun);   // unconditional, listed after anyToAttack

    anim::AnimParameters params;
    params.SetTrigger("attack");
    EXPECT_EQ(2, anim::EvaluateController(g, 0, 0.0f, params).toState);

    std::swap(g.transitions[1], g.transitions[2]);
    EXPECT_EQ(1, anim::EvaluateController(g, 0, 0.0f, params).toState);
}

TEST(Controller, AnyStateRespectsExitTimeOnTheCurrentState)
{
    anim::ControllerGraph g = GraphWithAnyState();
    g.transitions[1].hasExitTime = true;   // the Any State -> Attack transition
    g.transitions[1].exitTime = 0.5f;

    anim::AnimParameters params;
    params.SetTrigger("attack");

    // Exit time on an Any State transition measures the CURRENT state's progress,
    // since that is the only clip playing.
    EXPECT_FALSE(anim::EvaluateController(g, 1, 0.2f, params).fired);
    EXPECT_TRUE (anim::EvaluateController(g, 1, 0.7f, params).fired);
}

TEST(Controller, AnUnusableAnyStateTransitionDoesNotBlockTheCurrentStatesOwn)
{
    // Both reasons an Any State transition can be passed over -- it targets the
    // state already current, and it targets a state that was deleted -- must SKIP
    // rather than end the Any State pass. Otherwise one stale link in the editor
    // silently disables every transition authored below it, including the current
    // state's own, which reads as "the graph stopped working".
    anim::ControllerGraph g = GraphWithAnyState();

    anim::ControllerTransition anyToNowhere;
    anyToNowhere.fromState = anim::ControllerGraph::c_anyState;
    anyToNowhere.toState = 47;                  // a state the editor deleted
    g.transitions.insert(g.transitions.begin(), anyToNowhere);

    anim::AnimParameters params;   // no trigger, so Any State -> Attack is unsatisfied

    // Idle's own unconditional Idle -> Run must still be reached.
    const anim::TransitionResult r = anim::EvaluateController(g, 0, 0.0f, params);
    EXPECT_TRUE(r.fired);
    EXPECT_EQ(1, r.toState);
}

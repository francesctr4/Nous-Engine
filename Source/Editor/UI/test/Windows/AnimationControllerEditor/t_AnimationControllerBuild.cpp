#include <EditorUI/AnimationControllerBuild.h>

#include <gtest/gtest.h>

#include <vector>

namespace anim = nous::engine::animation_system;
namespace ed   = nous::anim_editor;

// =============================================================================
// BuildGraph -- canvas nodes and links into a ControllerGraph
// =============================================================================

TEST(t_AnimationControllerBuild, LinksBecomeTransitionsBetweenTheRightStates)
{
    std::vector<ed::BuildNode> nodes = {
        { 1, ed::BuildNodeKind::AnyState, {} },
        { 2, ed::BuildNodeKind::State, { "Idle" } },
        { 3, ed::BuildNodeKind::State, { "Run" } },
    };
    std::vector<ed::BuildLink> links = { { 2, 3, {} } };

    const anim::ControllerGraph g = ed::BuildGraph(nodes, links);

    ASSERT_EQ(2u, g.states.size());
    ASSERT_EQ(1u, g.transitions.size());
    EXPECT_EQ(0, g.transitions[0].fromState);
    EXPECT_EQ(1, g.transitions[0].toState);
}

TEST(t_AnimationControllerBuild, ALinkFromTheAnyStateNodeBecomesTheSentinel)
{
    std::vector<ed::BuildNode> nodes = {
        { 1, ed::BuildNodeKind::AnyState, {} },
        { 2, ed::BuildNodeKind::State, { "Attack" } },
    };
    std::vector<ed::BuildLink> links = { { 1, 2, {} } };

    const anim::ControllerGraph g = ed::BuildGraph(nodes, links);

    ASSERT_EQ(1u, g.transitions.size());
    EXPECT_EQ(anim::ControllerGraph::c_anyState, g.transitions[0].fromState);

    // The Any State node is NOT a state -- it must not appear in the state list, or
    // every index after it shifts and every transition points one state off.
    ASSERT_EQ(1u, g.states.size());
    EXPECT_EQ("Attack", g.states[0].name);
}

// Authored order IS the priority (design §3): the evaluator walks transitions in
// order and the first satisfied one wins. A build that sorted or grouped links --
// by source state, say, which is the obvious tidying -- would silently change which
// transition fires, with nothing on screen to show it.
TEST(t_AnimationControllerBuild, LinkOrderIsPreservedBecauseItDecidesWhichTransitionWins)
{
    std::vector<ed::BuildNode> nodes = {
        { 1, ed::BuildNodeKind::AnyState, {} },
        { 2, ed::BuildNodeKind::State, { "Idle" } },
        { 3, ed::BuildNodeKind::State, { "Run" } },
        { 4, ed::BuildNodeKind::State, { "Attack" } },
    };
    std::vector<ed::BuildLink> links = { { 2, 4, {} }, { 2, 3, {} } };

    const anim::ControllerGraph g = ed::BuildGraph(nodes, links);

    ASSERT_EQ(2u, g.transitions.size());
    EXPECT_EQ(2, g.transitions[0].toState);   // Attack, listed first
    EXPECT_EQ(1, g.transitions[1].toState);
}

// A link whose endpoint node was deleted is DROPPED, never emitted with a stale
// index: the canvas can hold one for a frame between the node going away and the
// link being cleaned up, and an out-of-range fromState would index the state array.
TEST(t_AnimationControllerBuild, ALinkToAMissingNodeIsDropped)
{
    std::vector<ed::BuildNode> nodes = {
        { 1, ed::BuildNodeKind::AnyState, {} },
        { 2, ed::BuildNodeKind::State, { "Idle" } },
    };
    std::vector<ed::BuildLink> links = {
        { 2, 99, {} },   // to a node that does not exist
        { 99, 2, {} },   // from one
    };

    const anim::ControllerGraph g = ed::BuildGraph(nodes, links);

    EXPECT_TRUE(g.transitions.empty());
    EXPECT_EQ(1u, g.states.size());
}

// =============================================================================
// Validate -- every authoring mistake the graph can express
// =============================================================================

TEST(t_AnimationControllerBuild, WarnsOnAStateWithNoClip)
{
    anim::ControllerGraph g;
    anim::ControllerState s;
    s.name = "Idle";
    s.clipIndex = -1;
    g.states.push_back(s);
    g.defaultState = 0;

    const auto warnings = ed::Validate(g);

    ASSERT_EQ(1u, warnings.size());
    EXPECT_EQ(ed::WarningKind::StateHasNoClip, warnings[0].kind);
    EXPECT_EQ("Idle", warnings[0].subject);
}

TEST(t_AnimationControllerBuild, WarnsOnAnUnresolvedDefaultState)
{
    anim::ControllerGraph g;
    anim::ControllerState s;
    s.name = "Idle";
    s.clipIndex = 0;
    g.states.push_back(s);
    g.defaultState = -1;

    const auto warnings = ed::Validate(g);

    // Exactly one: the reachability pass is GATED on a valid default, so a graph
    // with no default does not also report every state as an orphan. One cause, one
    // warning -- a panel that lists the same mistake N+1 times buries it.
    ASSERT_EQ(1u, warnings.size());
    EXPECT_EQ(ed::WarningKind::NoDefaultState, warnings[0].kind);
}

// The asymmetry that makes this warning worth having: an undeclared name reads
// through AnimParameters' fallback, so Greater, Less, IsTrue and TriggerSet are
// simply never satisfied -- but IsFalse IS. A mistyped name in an IsFalse condition
// produces a transition that always fires, which is invisible at runtime.
TEST(t_AnimationControllerBuild, WarnsOnAConditionNamingAnUndeclaredParameter)
{
    anim::ControllerGraph g;
    anim::ControllerState idle; idle.name = "Idle"; idle.clipIndex = 0;
    anim::ControllerState run;  run.name  = "Run";  run.clipIndex  = 1;
    g.states = { idle, run };
    g.defaultState = 0;

    anim::ControllerTransition t;
    t.fromState = 0; t.toState = 1;
    anim::ControllerCondition c;
    c.parameter  = "IsGrounded";       // declared as "isGrounded" -- case matters
    c.comparator = anim::ConditionComparator::IsTrue;
    t.conditions.push_back(c);
    g.transitions.push_back(t);

    g.parameters.push_back({ "isGrounded", 1, 0.0f });

    const auto warnings = ed::Validate(g);
    ASSERT_EQ(1u, warnings.size());
    EXPECT_EQ(ed::WarningKind::UndeclaredParameter, warnings[0].kind);
    EXPECT_EQ("IsGrounded", warnings[0].subject);
}

TEST(t_AnimationControllerBuild, WarnsOnAnUnconditionalTransition)
{
    anim::ControllerGraph g;
    anim::ControllerState idle; idle.name = "Idle"; idle.clipIndex = 0;
    anim::ControllerState run;  run.name  = "Run";  run.clipIndex  = 1;
    g.states = { idle, run };
    g.defaultState = 0;

    anim::ControllerTransition t;    // no conditions, no exit time
    t.fromState = 0; t.toState = 1;
    g.transitions.push_back(t);

    const auto warnings = ed::Validate(g);
    ASSERT_EQ(1u, warnings.size());
    EXPECT_EQ(ed::WarningKind::UnconditionalTransition, warnings[0].kind);
}

// An exit time is a condition in the sense that matters here: the transition does
// not fire the instant its source is entered, so the state is reachable and visible.
TEST(t_AnimationControllerBuild, AnExitTimeOnlyTransitionIsNotUnconditional)
{
    anim::ControllerGraph g;
    anim::ControllerState idle; idle.name = "Idle"; idle.clipIndex = 0;
    anim::ControllerState run;  run.name  = "Run";  run.clipIndex  = 1;
    g.states = { idle, run };
    g.defaultState = 0;

    anim::ControllerTransition t;
    t.fromState = 0; t.toState = 1;
    t.hasExitTime = true;
    t.exitTime    = 0.9f;
    g.transitions.push_back(t);

    EXPECT_TRUE(ed::Validate(g).empty());
}

TEST(t_AnimationControllerBuild, WarnsOnAStateUnreachableFromTheDefault)
{
    anim::ControllerGraph g;
    anim::ControllerState idle;   idle.name   = "Idle";   idle.clipIndex   = 0;
    anim::ControllerState orphan; orphan.name = "Orphan"; orphan.clipIndex = 1;
    g.states = { idle, orphan };
    g.defaultState = 0;

    const auto warnings = ed::Validate(g);
    ASSERT_EQ(1u, warnings.size());
    EXPECT_EQ(ed::WarningKind::UnreachableState, warnings[0].kind);
    EXPECT_EQ("Orphan", warnings[0].subject);
}

// Reachability must follow the Any State sentinel, or every attack state reachable
// only from Any State is reported as orphaned. A warning that fires on the CORRECT
// authoring is worse than no warning: it trains the user to ignore the panel.
TEST(t_AnimationControllerBuild, AnAnyStateTransitionMakesItsTargetReachable)
{
    anim::ControllerGraph g;
    anim::ControllerState idle;   idle.name   = "Idle";   idle.clipIndex   = 0;
    anim::ControllerState attack; attack.name = "Attack"; attack.clipIndex = 1;
    g.states = { idle, attack };
    g.defaultState = 0;

    anim::ControllerTransition t;
    t.fromState = anim::ControllerGraph::c_anyState;
    t.toState   = 1;
    anim::ControllerCondition c;
    c.parameter  = "attack";
    c.comparator = anim::ConditionComparator::TriggerSet;
    t.conditions.push_back(c);
    g.transitions.push_back(t);
    g.parameters.push_back({ "attack", 2, 0.0f });

    EXPECT_TRUE(ed::Validate(g).empty());
}

// Reachability is TRANSITIVE: a state two hops from the default is reachable. A
// one-hop-only check would report every real graph's later states as orphans.
TEST(t_AnimationControllerBuild, ReachabilityFollowsTheWholeChain)
{
    anim::ControllerGraph g;
    anim::ControllerState a; a.name = "A"; a.clipIndex = 0;
    anim::ControllerState b; b.name = "B"; b.clipIndex = 1;
    anim::ControllerState c; c.name = "C"; c.clipIndex = 2;
    g.states = { a, b, c };
    g.defaultState = 0;

    const auto edge = [&g](int from, int to)
    {
        anim::ControllerTransition t;
        t.fromState = from;
        t.toState   = to;
        t.hasExitTime = true;          // so it is not reported as unconditional
        g.transitions.push_back(t);
    };

    edge(0, 1);
    edge(1, 2);

    EXPECT_TRUE(ed::Validate(g).empty());
}

TEST(t_AnimationControllerBuild, AWellFormedGraphProducesNoWarnings)
{
    anim::ControllerGraph g;
    anim::ControllerState idle; idle.name = "Idle"; idle.clipIndex = 0;
    anim::ControllerState run;  run.name  = "Run";  run.clipIndex  = 1;
    g.states = { idle, run };
    g.defaultState = 0;

    anim::ControllerTransition t;
    t.fromState = 0; t.toState = 1;
    anim::ControllerCondition c;
    c.parameter  = "speed";
    c.comparator = anim::ConditionComparator::Greater;
    c.value      = 0.1f;
    t.conditions.push_back(c);
    g.transitions.push_back(t);
    g.parameters.push_back({ "speed", 0, 0.0f });

    EXPECT_TRUE(ed::Validate(g).empty());
}

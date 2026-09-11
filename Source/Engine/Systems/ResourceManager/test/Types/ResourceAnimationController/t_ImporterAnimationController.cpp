#include <ResourceManager/Types/ResourceAnimationController/ImporterAnimationController.h>
#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>
#include <MemoryManager/MemoryManager.h>

#include <gtest/gtest.h>

#include <string>

namespace anim = nous::engine::animation_system;

namespace
{
    const std::string c_path = "t_AnimationController_roundtrip.nctrl";

    // A graph exercising every field that must survive the round trip: both
    // transition sources, every comparator kind, a speed parameter, a non-default
    // root motion mode, and a parameter declaration of each type.
    anim::ControllerGraph SampleGraph()
    {
        anim::ControllerGraph g;

        anim::ControllerState idle;
        idle.name = "Idle";
        idle.clipIndex = 0;
        g.states.push_back(idle);

        anim::ControllerState run;
        run.name = "Run";
        run.clipIndex = 1;
        run.speed = 1.25f;
        run.speedParameter = "speed";
        run.rootMotion = 2;            // Applied -- Inherit is 0, Baked 1
        g.states.push_back(run);

        anim::ControllerTransition toRun;
        toRun.fromState = 0;
        toRun.toState = 1;
        toRun.duration = 0.15f;
        anim::ControllerCondition fast;
        fast.parameter = "speed";
        fast.comparator = anim::ConditionComparator::Greater;
        fast.value = 0.1f;
        toRun.conditions.push_back(fast);
        g.transitions.push_back(toRun);

        anim::ControllerTransition anyToIdle;
        anyToIdle.fromState = anim::ControllerGraph::c_anyState;
        anyToIdle.toState = 0;
        anyToIdle.hasExitTime = true;
        anyToIdle.exitTime = 0.9f;
        anim::ControllerCondition stop;
        stop.parameter = "halt";
        stop.comparator = anim::ConditionComparator::TriggerSet;
        anyToIdle.conditions.push_back(stop);
        g.transitions.push_back(anyToIdle);

        g.parameters.push_back({ "speed", 0, 0.0f });
        g.parameters.push_back({ "isGrounded", 1, 1.0f });
        g.parameters.push_back({ "halt", 2, 0.0f });

        g.defaultState = 0;
        return g;
    }
}

class ImporterAnimationControllerTest : public ::testing::Test
{
protected:
    void SetUp() override { nous::engine::memory::InitializeMemory(MiB(16)); }
    void TearDown() override { nous::engine::memory::ShutdownMemory(); }
};

TEST_F(ImporterAnimationControllerTest, GraphSurvivesAWriteReadRoundTrip)
{
    ResourceAnimationController written(1);
    written.graph = SampleGraph();

    ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(written, c_path));

    ResourceAnimationController read(2);
    ImporterAnimationController importer;
    ASSERT_TRUE(importer.Deserialize(c_path, &read));

    const anim::ControllerGraph& g = read.graph;

    ASSERT_EQ(2u, g.states.size());
    EXPECT_EQ("Idle", g.states[0].name);
    EXPECT_EQ("Run",  g.states[1].name);
    EXPECT_FLOAT_EQ(1.25f, g.states[1].speed);
    EXPECT_EQ("speed", g.states[1].speedParameter);
    EXPECT_EQ(2, g.states[1].rootMotion);
    EXPECT_EQ(0, g.defaultState);

    ASSERT_EQ(2u, g.transitions.size());
    EXPECT_EQ(0, g.transitions[0].fromState);
    EXPECT_EQ(1, g.transitions[0].toState);
    EXPECT_FLOAT_EQ(0.15f, g.transitions[0].duration);
    ASSERT_EQ(1u, g.transitions[0].conditions.size());
    EXPECT_EQ("speed", g.transitions[0].conditions[0].parameter);
    EXPECT_EQ(anim::ConditionComparator::Greater, g.transitions[0].conditions[0].comparator);
    EXPECT_FLOAT_EQ(0.1f, g.transitions[0].conditions[0].value);

    // The Any State sentinel must survive as a sentinel, not as a state index.
    EXPECT_EQ(anim::ControllerGraph::c_anyState, g.transitions[1].fromState);
    EXPECT_TRUE(g.transitions[1].hasExitTime);
    EXPECT_FLOAT_EQ(0.9f, g.transitions[1].exitTime);
    EXPECT_EQ(anim::ConditionComparator::TriggerSet, g.transitions[1].conditions[0].comparator);

    ASSERT_EQ(3u, g.parameters.size());
    EXPECT_EQ("speed", g.parameters[0].name);
    EXPECT_EQ(1, g.parameters[1].type);
    EXPECT_FLOAT_EQ(1.0f, g.parameters[1].defaultValue);
}

TEST_F(ImporterAnimationControllerTest, EndpointsAreNamesSoReorderingStatesDoesNotRepointTransitions)
{
    // The whole reason endpoints serialize as NAMES: an index written to disk is
    // invalidated by any reorder, and the failure is silent -- the transition still
    // points somewhere, just at the wrong state.
    ResourceAnimationController written(1);
    written.graph = SampleGraph();

    // Idle(0) -> Run(1) becomes Run(0) -> Idle(1) positionally.
    std::swap(written.graph.states[0], written.graph.states[1]);
    written.graph.transitions[0].fromState = 1;   // still Idle
    written.graph.transitions[0].toState   = 0;   // still Run
    written.graph.transitions[1].toState   = 1;   // AnyState -> Idle
    written.graph.defaultState             = 1;   // still Idle

    const std::string path = "t_AnimationController_reordered.nctrl";
    ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(written, path));

    ResourceAnimationController read(2);
    ImporterAnimationController importer;
    ASSERT_TRUE(importer.Deserialize(path, &read));

    ASSERT_EQ(2u, read.graph.states.size());
    EXPECT_EQ("Run",  read.graph.states[0].name);
    EXPECT_EQ("Idle", read.graph.states[1].name);

    EXPECT_EQ(1, read.graph.transitions[0].fromState);   // Idle, wherever it landed
    EXPECT_EQ(0, read.graph.transitions[0].toState);     // Run
    EXPECT_EQ(1, read.graph.defaultState);
}

TEST_F(ImporterAnimationControllerTest, ABrokenTransitionEndpointLoadsAsMinusOneRatherThanFailing)
{
    // A link the editor left pointing at a deleted state must cost that link, not
    // the whole asset -- EvaluateController skips a -1 target, and the editor
    // reports it.
    ResourceAnimationController written(1);
    written.graph = SampleGraph();
    written.graph.transitions[0].toState = 99;   // resolves to no name on write

    const std::string path = "t_AnimationController_broken.nctrl";
    ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(written, path));

    ResourceAnimationController read(2);
    ImporterAnimationController importer;
    ASSERT_TRUE(importer.Deserialize(path, &read));

    ASSERT_EQ(2u, read.graph.transitions.size());
    EXPECT_EQ(-1, read.graph.transitions[0].toState);
    EXPECT_EQ(0,  read.graph.transitions[0].fromState);   // the good end is intact
}

TEST_F(ImporterAnimationControllerTest, AnUnresolvedClipSlotSurvivesASaveInsteadOfBeingBlanked)
{
    // The write path reads the AUTHORED slot, not just the resolved pointer. A
    // state whose .nanim is missing resolves to null; if saving read only the
    // pointer, the next save would erase the binding and the user could never see
    // what it had been pointing at.
    ResourceAnimationController written(1);
    anim::ControllerState idle;
    idle.name = "Idle";
    written.graph.states.push_back(idle);
    written.clipSlots.push_back({ "Assets/Gone.nanim", "Library/Animations/7.nanim", 7u });

    const std::string path = "t_AnimationController_orphanclip.nctrl";
    ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(written, path));

    ResourceAnimationController read(2);
    ImporterAnimationController importer;
    ASSERT_TRUE(importer.Deserialize(path, &read));

    ASSERT_EQ(1u, read.clipSlots.size());
    EXPECT_EQ("Assets/Gone.nanim", read.clipSlots[0].assetPath);
    EXPECT_EQ(7u, read.clipSlots[0].uid);

    // Still unplayable, because nothing resolved it -- broken, not blank.
    EXPECT_EQ(-1, read.graph.states[0].clipIndex);
}

TEST_F(ImporterAnimationControllerTest, ANewFileLoadsAsAnEmptyGraph)
{
    const std::string path = "t_AnimationController_new.nctrl";
    ASSERT_TRUE(ImporterAnimationController::CreateNewControllerFile(path));

    ResourceAnimationController read(3);
    ImporterAnimationController importer;
    ASSERT_TRUE(importer.Deserialize(path, &read));

    EXPECT_TRUE(read.graph.states.empty());
    EXPECT_TRUE(read.graph.transitions.empty());
    EXPECT_EQ(-1, read.graph.defaultState);
}

TEST_F(ImporterAnimationControllerTest, MissingKeysReadAsDefaultsNotFailures)
{
    // An asset hand-edited down to the minimum must still load. Absent keys mean
    // DEFAULT, never corrupt -- the same rule the .nanim stub follows.
    ResourceAnimationController written(4);
    anim::ControllerState bare;
    bare.name = "Bare";
    written.graph.states.push_back(bare);

    const std::string path = "t_AnimationController_bare.nctrl";
    ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(written, path));

    ResourceAnimationController read(5);
    ImporterAnimationController importer;
    ASSERT_TRUE(importer.Deserialize(path, &read));

    ASSERT_EQ(1u, read.graph.states.size());
    EXPECT_FLOAT_EQ(1.0f, read.graph.states[0].speed);
    EXPECT_TRUE(read.graph.states[0].speedParameter.empty());
    EXPECT_EQ(0, read.graph.states[0].rootMotion);      // Inherit
    EXPECT_EQ(-1, read.graph.states[0].clipIndex);
}

TEST_F(ImporterAnimationControllerTest, DeserializingTwiceDoesNotAccumulateStatesOrTransitions)
{
    // Deserialize runs on a LIVE resource during asset hot-reload. Every container
    // it fills must be cleared first, or a re-read doubles the graph and every
    // authored transition index still resolves -- so it looks like it worked.
    ResourceAnimationController written(1);
    written.graph = SampleGraph();
    ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(written, c_path));

    ResourceAnimationController read(2);
    ImporterAnimationController importer;

    ASSERT_TRUE(importer.Deserialize(c_path, &read));
    ASSERT_TRUE(importer.Deserialize(c_path, &read));

    EXPECT_EQ(2u, read.graph.states.size());
    EXPECT_EQ(2u, read.graph.transitions.size());
    EXPECT_EQ(3u, read.graph.parameters.size());
    EXPECT_EQ(2u, read.clipSlots.size());
    EXPECT_EQ(2u, read.editorPositions.size());
}

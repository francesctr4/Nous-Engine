#include <ResourceManager/Types/ResourceAnimationController/ImporterAnimationController.h>
#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <MemoryManager/MemoryManager.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

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

    // A ref-counting stand-in for ModuleResourceManager.
    //
    // A REAL manager is not an option here and that is structural, not laziness:
    // ModuleResourceManager lives under Engine/Modules/, and nothing in Systems/
    // may include Modules/ -- the rule check_header_layout.py enforces. IResourceLoader
    // is precisely the seam that exists for this, so the fake implements it and
    // moves real ResourceBase reference counts, which is what the invariant is about.
    struct FakeClipLoader : IResourceLoader
    {
        std::map<uint32_t, std::unique_ptr<ResourceAnimation>> clips;

        int creates = 0;
        int unloads = 0;

        ResourceAnimation& Add(const uint32_t uid, const std::string& assetPath)
        {
            auto clip = std::make_unique<ResourceAnimation>(uid);
            clip->SetAssetsPath(assetPath);
            clip->SetLibraryPath("Library/Animations/" + std::to_string(uid) + ".nanim");

            ResourceAnimation& ref = *clip;
            clips[uid] = std::move(clip);
            return ref;
        }

        [[nodiscard]] uint32_t RefCount(const uint32_t uid) const
        {
            const auto it = clips.find(uid);
            return it == clips.end() ? 0u : it->second->GetReferenceCount();
        }

        // Both acquire paths take a reference, exactly as the real manager does --
        // which is what makes "the importer must give one back" testable at all.
        ResourceBase* CreateResourceFromLibrary(const uint32_t uid, ResourceType, const std::string&,
                                                const std::string&, const std::string&) override
        {
            const auto it = clips.find(uid);
            if (it == clips.end()) return nullptr;

            ++creates;
            it->second->IncreaseReferenceCount();
            return it->second.get();
        }

        ResourceBase* CreateResource(const std::string& assetsPath) override
        {
            for (auto& [uid, clip] : clips)
                if (clip->GetAssetsPath() == assetsPath)
                {
                    ++creates;
                    clip->IncreaseReferenceCount();
                    return clip.get();
                }

            return nullptr;
        }

        bool UnloadResource(const uint32_t uid) override
        {
            const auto it = clips.find(uid);
            if (it == clips.end()) return false;

            ++unloads;
            it->second->DecreaseReferenceCount();
            return true;
        }

        ResourceMesh* RequestOrCreateSubMeshResource(const std::string&, int32_t) override
        { return nullptr; }
        ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(const std::string&, int32_t,
                                                                const std::string&, uint32_t) override
        { return nullptr; }
        ResourceMaterial* GetDefaultMaterial() const override { return nullptr; }
        bool ImportFile(const std::string&) override { return true; }
    };

    // A two-state controller whose clips are already enriched with uid + libraryPath,
    // i.e. what a Library copy looks like after Save.
    void WriteResolvableController(const std::string& path)
    {
        ResourceAnimationController c(1);
        c.graph = SampleGraph();
        c.clipSlots.push_back({ "Assets/Idle.nanim", "Library/Animations/11.nanim", 11u });
        c.clipSlots.push_back({ "Assets/Run.nanim",  "Library/Animations/12.nanim", 12u });

        ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(c, path));
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

// ---------------------------------------------------------------------------
// Clip references -- the hazard this feature is most likely to reintroduce
// ---------------------------------------------------------------------------

TEST_F(ImporterAnimationControllerTest, DeserializeAcquiresOneReferencePerResolvedClip)
{
    const std::string path = "t_AnimationController_refs.nctrl";
    WriteResolvableController(path);

    FakeClipLoader loader;
    loader.Add(11u, "Assets/Idle.nanim");
    loader.Add(12u, "Assets/Run.nanim");

    ResourceAnimationController controller(2);
    ImporterAnimationController importer;
    importer.m_resources = &loader;

    ASSERT_TRUE(importer.Deserialize(path, &controller));

    ASSERT_EQ(2u, controller.clips.size());
    ASSERT_NE(nullptr, controller.clips[0]);
    ASSERT_NE(nullptr, controller.clips[1]);

    EXPECT_EQ(1u, loader.RefCount(11u));
    EXPECT_EQ(1u, loader.RefCount(12u));

    // clipIndex is DERIVED here, never serialized -- a state is playable only once
    // something actually resolved its clip.
    EXPECT_EQ(0, controller.graph.states[0].clipIndex);
    EXPECT_EQ(1, controller.graph.states[1].clipIndex);
}

TEST_F(ImporterAnimationControllerTest, ReDeserializingALiveControllerDoesNotChangeClipRefCounts)
{
    // THE test of this task. Deserialize is not called only on a fresh resource:
    // the asset hot-reload path re-deserializes a LIVE controller in place, so each
    // pass re-acquires every clip and must release what the slots already held.
    //
    // The `previous != clip` guard is the obvious version and leaks in exactly this
    // case -- re-resolving finds the SAME clip resident and only increments, so a
    // change-detecting release never fires and the count climbs by one per reload.
    const std::string path = "t_AnimationController_refs.nctrl";
    WriteResolvableController(path);

    FakeClipLoader loader;
    loader.Add(11u, "Assets/Idle.nanim");
    loader.Add(12u, "Assets/Run.nanim");

    ResourceAnimationController controller(2);
    ImporterAnimationController importer;
    importer.m_resources = &loader;

    ASSERT_TRUE(importer.Deserialize(path, &controller));
    const uint32_t afterFirst = loader.RefCount(11u);

    ASSERT_TRUE(importer.Deserialize(path, &controller));
    ASSERT_TRUE(importer.Deserialize(path, &controller));

    EXPECT_EQ(afterFirst, loader.RefCount(11u)) << "a re-Deserialize leaked a reference per clip";
    EXPECT_EQ(afterFirst, loader.RefCount(12u));

    // Never zero in between, either: the release runs AFTER the acquire, so the
    // count cannot dip and queue a spurious eviction. Three passes acquired three
    // times and gave back the two the earlier passes held.
    EXPECT_EQ(6, loader.creates);
    EXPECT_EQ(4, loader.unloads);
}

TEST_F(ImporterAnimationControllerTest, EvictGivesBackEveryClipReference)
{
    const std::string path = "t_AnimationController_refs.nctrl";
    WriteResolvableController(path);

    FakeClipLoader loader;
    loader.Add(11u, "Assets/Idle.nanim");
    loader.Add(12u, "Assets/Run.nanim");

    ResourceAnimationController controller(2);
    ImporterAnimationController importer;
    importer.m_resources = &loader;

    ASSERT_TRUE(importer.Deserialize(path, &controller));
    ASSERT_EQ(1u, loader.RefCount(11u));

    importer.Evict(&controller);

    EXPECT_EQ(0u, loader.RefCount(11u));
    EXPECT_EQ(0u, loader.RefCount(12u));

    for (const ResourceAnimation* clip : controller.clips)
        EXPECT_EQ(nullptr, clip);

    // A second Evict must be a no-op, not a double-release: the pointers were
    // nulled, which is what stands between a double teardown and a count going
    // negative.
    importer.Evict(&controller);
    EXPECT_EQ(2, loader.unloads);
}

TEST_F(ImporterAnimationControllerTest, AStateWhoseClipIsMissingStaysUnplayableWithoutDisturbingItsPeers)
{
    // One broken slot must cost that state, not the controller. The peer still
    // resolves and still takes exactly one reference.
    const std::string path = "t_AnimationController_refs.nctrl";
    WriteResolvableController(path);

    FakeClipLoader loader;
    loader.Add(12u, "Assets/Run.nanim");   // 11 is deliberately absent

    ResourceAnimationController controller(2);
    ImporterAnimationController importer;
    importer.m_resources = &loader;

    ASSERT_TRUE(importer.Deserialize(path, &controller));

    EXPECT_EQ(nullptr, controller.clips[0]);
    EXPECT_EQ(-1, controller.graph.states[0].clipIndex);

    ASSERT_NE(nullptr, controller.clips[1]);
    EXPECT_EQ(1, controller.graph.states[1].clipIndex);
    EXPECT_EQ(1u, loader.RefCount(12u));

    // And the authored slot is still there to be repaired.
    EXPECT_EQ("Assets/Idle.nanim", controller.clipSlots[0].assetPath);
}

TEST_F(ImporterAnimationControllerTest, ARemovedStateReleasesItsClipOnTheNextDeserialize)
{
    // The state count can change between passes -- the editor deletes a node and
    // saves. A per-slot release keyed on index would silently strand the reference
    // held by a state that no longer exists.
    const std::string path = "t_AnimationController_refs.nctrl";
    WriteResolvableController(path);

    FakeClipLoader loader;
    loader.Add(11u, "Assets/Idle.nanim");
    loader.Add(12u, "Assets/Run.nanim");

    ResourceAnimationController controller(2);
    ImporterAnimationController importer;
    importer.m_resources = &loader;

    ASSERT_TRUE(importer.Deserialize(path, &controller));
    ASSERT_EQ(1u, loader.RefCount(12u));

    // Rewrite the asset with Run deleted, then re-read in place.
    {
        ResourceAnimationController shrunk(3);
        shrunk.graph.states.push_back(controller.graph.states[0]);
        shrunk.graph.states[0].clipIndex = -1;
        shrunk.graph.defaultState = 0;
        shrunk.clipSlots.push_back({ "Assets/Idle.nanim", "Library/Animations/11.nanim", 11u });
        ASSERT_TRUE(ImporterAnimationController::WriteControllerToFile(shrunk, path));
    }

    ASSERT_TRUE(importer.Deserialize(path, &controller));

    EXPECT_EQ(1u, controller.clips.size());
    EXPECT_EQ(1u, loader.RefCount(11u));
    EXPECT_EQ(0u, loader.RefCount(12u)) << "the deleted state's clip reference was stranded";
}

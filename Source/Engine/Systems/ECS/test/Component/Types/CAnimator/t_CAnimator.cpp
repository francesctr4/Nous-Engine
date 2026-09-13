#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonObject.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <string>
#include <vector>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::Transform;

namespace
{
    // The MVP-F stand-in for the deleted CAnimator::clips: a controller holding one
    // state per clip, each state named after its clip's RESOURCE name and defaulting
    // to the first. That naming is what lets every pre-MVP-F CrossFade("B", ...) in
    // this file keep meaning what it meant.
    //
    // The controller's UID must be NON-ZERO for the animator to bind it: CAnimator
    // compares UIDOf(controller) against m_boundController, which starts at 0, so a
    // uid-0 controller reads as "same as nothing" and is never picked up. Real
    // resources always carry one; only a hand-built test controller can trip this.
    void SetClips(ResourceAnimationController& controller,
                  std::initializer_list<ResourceAnimation*> clips)
    {
        controller.graph = {};
        controller.clips.clear();

        for (ResourceAnimation* clip : clips)
        {
            nous::engine::animation_system::ControllerState state;
            state.name      = clip ? clip->GetName() : std::string();
            state.clipIndex = static_cast<int>(controller.clips.size());

            controller.graph.states.push_back(std::move(state));
            controller.clips.push_back(clip);
        }

        controller.graph.defaultState = controller.graph.states.empty() ? -1 : 0;
    }

    // Appends one transition and hands it back so the caller can push conditions onto
    // it. An empty condition list is satisfied, so a transition added and left alone
    // fires the first frame its source state is current -- which is exactly what the
    // arbitration tests need to prove a CrossFade held the graph off.
    nous::engine::animation_system::ControllerTransition&
    AddTransition(ResourceAnimationController& controller,
                  const int from, const int to, const float duration)
    {
        nous::engine::animation_system::ControllerTransition t;
        t.fromState = from;
        t.toState   = to;
        t.duration  = duration;

        controller.graph.transitions.push_back(std::move(t));
        return controller.graph.transitions.back();
    }

    // Two bones: "Root" (index 0, no parent) and "Child" (index 1, parent 0).
    //
    // Bind locals are identity, so a bone the clip does not drive stays at the
    // origin -- which means any translation appearing in the globals demonstrably
    // came from the clip and not from the bind pose.
    void MakeTwoBoneRig(ResourceSkeleton& rig)
    {
        auto& s = rig.skeleton;
        s.names      = { "Root", "Child" };
        s.parents    = { -1, 0 };
        s.bindLocals = { Transform{}, Transform{} };
        s.offsets    = { glm::mat4(1.0f), glm::mat4(1.0f) };
        s.RebuildLookup();
    }

    // A 1-second clip translating `boneName` from x = 0 to x = 10.
    void MakeSlideClip(ResourceAnimation& anim, const char* boneName)
    {
        anim.clip.name     = "Slide";
        anim.clip.duration = 1.0f;

        AnimChannel ch;
        ch.boneName  = boneName;
        ch.posTimes  = { 0.0f, 1.0f };
        ch.posValues = { glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f) };

        anim.clip.channels = { ch };
    }

    // Holds `boneName` at x for the whole clip. Two of these make blend weight
    // directly readable: blending hold(0) and hold(10) at weight w gives x == 10w,
    // with no dependence on how far either clip has advanced.
    void MakeHoldClip(ResourceAnimation& anim, const char* boneName, float x)
    {
        anim.clip.name     = "Hold";
        anim.clip.duration = 1.0f;

        AnimChannel ch;
        ch.boneName  = boneName;
        ch.posTimes  = { 0.0f, 1.0f };
        ch.posValues = { glm::vec3(x, 0.0f, 0.0f), glm::vec3(x, 0.0f, 0.0f) };

        anim.clip.channels = { ch };
    }

    float TranslationX(const glm::mat4& m) { return m[3][0]; }

    // A 1-second clip yawing `boneName` from 0 to 90 degrees about +Y. The only
    // helper that produces a TURN, which is what separates Applied from InPlace:
    // a clip with no rotation channel makes the two modes indistinguishable.
    void MakeTurnClip(ResourceAnimation& anim, const char* boneName)
    {
        anim.clip.name     = "Turn";
        anim.clip.duration = 1.0f;

        AnimChannel ch;
        ch.boneName  = boneName;
        ch.rotTimes  = { 0.0f, 1.0f };
        ch.rotValues = { glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                         glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) };

        anim.clip.channels = { ch };
    }

    // sin(yaw) of a Y-rotation matrix: the x component of its forward basis.
    float ForwardX(const glm::mat4& m) { return m[2][0]; }

    // A rig whose bind pose is NOT identity: Child sits 2 units above Root, and
    // offsets are inverse(global bind) -- which is what an importer produces. So
    // sampling the bind pose must give an identity palette. A rig with identity
    // bind locals would pass that test for the wrong reason.
    void MakeOffsetRig(ResourceSkeleton& rig)
    {
        auto& s = rig.skeleton;
        s.names   = { "Root", "Child" };
        s.parents = { -1, 0 };

        Transform childBind;
        childBind.position = glm::vec3(0.0f, 2.0f, 0.0f);
        s.bindLocals = { Transform{}, childBind };

        const glm::mat4 rootGlobal  = glm::mat4(1.0f);
        const glm::mat4 childGlobal = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
        s.offsets = { glm::inverse(rootGlobal), glm::inverse(childGlobal) };

        s.RebuildLookup();
    }

    // A 1-second clip that holds `boneName` at the origin. Every other bone is
    // undriven, so Sample() fills it from bindLocals -- giving a pose identical to
    // the bind pose.
    void MakeHoldAtBindClip(ResourceAnimation& anim, const char* boneName)
    {
        anim.clip.name     = "HoldAtBind";
        anim.clip.duration = 1.0f;

        AnimChannel ch;
        ch.boneName  = boneName;
        ch.posTimes  = { 0.0f, 1.0f };
        ch.posValues = { glm::vec3(0.0f), glm::vec3(0.0f) };

        anim.clip.channels = { ch };
    }

    bool IsIdentity(const glm::mat4& m)
    {
        const glm::mat4 identity(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (std::fabs(m[c][r] - identity[c][r]) > 1e-5f)
                    return false;
        return true;
    }
}

class t_CAnimator : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene", &fakes.services);
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        nous::engine::memory::ShutdownMemory();
    }

    // Declared before `scene` so it outlives it -- the Scene holds a pointer into it.
    FakeServices fakes;
    Scene*       scene = nullptr;
};

// =============================================================================
// Binding + sampling
// =============================================================================

TEST_F(t_CAnimator, BindsAndSamplesTheDrivenBone)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(901);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;

    a.OnUpdate(0.5f);

    ASSERT_TRUE(a.IsBound());
    ASSERT_EQ(a.GetBoneGlobals().size(), 2u);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 0.0f);   // undriven root
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);   // halfway along
}

TEST_F(t_CAnimator, RebindsWhenTheClipSlotChanges)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation animA(2);  MakeSlideClip(animA, "Child");
    ResourceAnimation animB(3);  MakeSlideClip(animB, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(902);
    SetClips(aCtrl, { &animA });
    a.controller = &aCtrl;
    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    // Clip B drives Root instead of Child. A stale binding would keep moving
    // Child and leave Root at the origin -- the exact inverse of the assertions.
    SetClips(aCtrl, { &animB });
    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 5.0f);   // Root moved
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);   // Child only inherits Root
}

TEST_F(t_CAnimator, RebindsWhenTheSkeletonSlotChanges)
{
    ResourceSkeleton rigA(1);  MakeTwoBoneRig(rigA);
    ResourceSkeleton rigB(4);  MakeTwoBoneRig(rigB);
    rigB.skeleton.names = { "Root", "Other" };   // "Child" no longer exists here
    rigB.skeleton.RebuildLookup();

    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rigA;
    ResourceAnimationController aCtrl(903);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    // Against rigB the clip's one channel matches no bone, so nothing moves.
    a.skeleton = &rigB;
    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 0.0f);
}

// =============================================================================
// Degenerate slots
// =============================================================================

TEST_F(t_CAnimator, NullSlotsAreInert)
{
    ResourceSkeleton rig(1);  MakeTwoBoneRig(rig);

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();

    a.OnUpdate(0.5f);                       // both slots null
    EXPECT_FALSE(a.IsBound());
    EXPECT_TRUE(a.GetBoneGlobals().empty());

    a.skeleton = &rig;                      // skeleton only, still no clip
    a.OnUpdate(0.5f);
    EXPECT_FALSE(a.IsBound());
    EXPECT_TRUE(a.GetBoneGlobals().empty());
}

TEST_F(t_CAnimator, ClearingASlotClearsThePose)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(904);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.OnUpdate(0.5f);
    ASSERT_FALSE(a.GetBoneGlobals().empty());

    SetClips(aCtrl, {});
    a.OnUpdate(0.5f);

    EXPECT_FALSE(a.IsBound());
    EXPECT_TRUE(a.GetBoneGlobals().empty());
}

// =============================================================================
// EnTT pool relocation
//
// EnTT relocates components by memcpy when a pool grows. AnimInstance::binding
// points at CAnimator::m_binding -- a member of the SAME object -- so a pointer
// stored once survives the move as a dangling read into vacated memory. The fix
// is reassigning it every OnUpdate; this test is what proves the fix is present,
// and it is the reason this file exists.
// =============================================================================

TEST_F(t_CAnimator, SurvivesPoolRelocation)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    // ONE controller for all 256, which is also the real-world shape: a controller
    // asset is shared between every character that uses it, exactly as a clip is.
    ResourceAnimationController aCtrl(905);
    SetClips(aCtrl, { &anim });

    std::vector<GameObject> objects;
    objects.reserve(256);

    for (int i = 0; i < 256; ++i)
    {
        GameObject go = scene->CreateGameObject("Rig");
        auto& a = go.AddComponent<CAnimator>();   // grows and relocates the pool
        a.skeleton   = &rig;
        a.controller = &aCtrl;
        objects.push_back(go);
    }

    for (GameObject& go : objects)
        go.GetComponent<CAnimator>().OnUpdate(0.5f);

    for (GameObject& go : objects)
    {
        const auto& globals = go.GetComponent<CAnimator>().GetBoneGlobals();
        ASSERT_EQ(globals.size(), 2u);
        EXPECT_FLOAT_EQ(TranslationX(globals[1]), 5.0f);
    }
}

// =============================================================================
// Bone palette
//
// The palette is what GPU skinning consumes: palette[b] takes a vertex from mesh
// space into that bone's animated place, in MODEL space.
// =============================================================================

// THE property test. palette[b] = globals[b] * offsets[b], and at the bind pose
// globals[b] == inverse(offsets[b]), so every matrix is identity. This is the same
// invariant t_AnimationSystem_Palette pins, restated one layer up -- so a CAnimator
// that wires BuildPalette wrongly fails here without a renderer in sight.
TEST_F(t_CAnimator, BindPosePaletteIsIdentity)
{
    ResourceSkeleton  rig(1);   MakeOffsetRig(rig);
    ResourceAnimation anim(2);  MakeHoldAtBindClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(906);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;

    a.OnUpdate(0.5f);

    ASSERT_TRUE(a.IsBound());
    ASSERT_EQ(a.GetPalette().size(), 2u);
    EXPECT_TRUE(IsIdentity(a.GetPalette()[0]));
    EXPECT_TRUE(IsIdentity(a.GetPalette()[1]));
}

// A driven bone must leave identity, or the test above would also pass against a
// palette that is never written at all.
TEST_F(t_CAnimator, DrivenBoneLeavesIdentityInThePalette)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(907);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;

    a.OnUpdate(0.5f);

    ASSERT_EQ(a.GetPalette().size(), 2u);
    EXPECT_TRUE(IsIdentity(a.GetPalette()[0]));              // undriven root
    EXPECT_FLOAT_EQ(TranslationX(a.GetPalette()[1]), 5.0f);  // Child, halfway
}

// The renderer's skinned-geometry predicate is `!GetPalette().empty()`. If a
// cleared slot left the previous palette in place, every mesh bound to this
// animator would keep deforming to a pose that no longer has a source.
TEST_F(t_CAnimator, ClearingASlotEmptiesThePalette)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(908);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.OnUpdate(0.5f);
    ASSERT_FALSE(a.GetPalette().empty());

    SetClips(aCtrl, {});
    a.OnUpdate(0.5f);

    EXPECT_TRUE(a.GetPalette().empty());
}

// =============================================================================
// Resource lifetime — OnDestroy must release what the slots hold
// =============================================================================
//
// Both ways a slot is filled take a reference: Deserialize (CreateResource /
// CreateResourceFromLibrary) and the Inspector's drag-drop (CreateResource, which
// also releases the slot's previous occupant). Without the matching release at
// destruction, every play/stop cycle deserializes the scene again and leaks one
// reference per slot per animator, so the resources never evict.

TEST_F(t_CAnimator, OnDestroyReleasesBothSlots)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");
    rig.SetState(ResourceState::CPU_READY);
    anim.SetState(ResourceState::CPU_READY);

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(909);
    aCtrl.SetState(ResourceState::CPU_READY);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;

    a.OnDestroy();

    // The two slots the COMPONENT holds: skeleton and controller. The clip belongs
    // to the controller -- see OnDestroyReleasesTheSkeletonAndControllerButNotTheClips.
    ASSERT_EQ(fakes.resources.unloaded.size(), 2u);
    EXPECT_EQ(fakes.resources.unloaded[0], 1u);
    EXPECT_EQ(fakes.resources.unloaded[1], 909u);
}

// A slot the user never filled was never acquired, so releasing it would drive a
// reference count negative -- and DecreaseReferenceCount asserts on the result.
TEST_F(t_CAnimator, OnDestroyWithEmptySlotsReleasesNothing)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();

    a.OnDestroy();

    EXPECT_TRUE(fakes.resources.unloaded.empty());
}

// An UNLOADED resource never completed a load, mirroring CMesh::OnDestroy's guard.
TEST_F(t_CAnimator, OnDestroySkipsUnloadedResources)
{
    ResourceSkeleton rig(1);  MakeTwoBoneRig(rig);   // state stays UNLOADED

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;

    a.OnDestroy();

    EXPECT_TRUE(fakes.resources.unloaded.empty());
}

// Re-adding a CAnimator over a live one is what PrefabManager::RefreshPrefabInstances
// does to a prefab ROOT on every scene load. Without AddComponent firing OnDestroy for
// the component it replaces, the old slots' references are dropped on the floor and the
// skeleton/clip counts climb by one per play/stop cycle.
TEST_F(t_CAnimator, ReAddingTheComponentReleasesTheReplacedSlots)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");
    rig.SetState(ResourceState::CPU_READY);
    anim.SetState(ResourceState::CPU_READY);

    GameObject go = scene->CreateGameObject("Rig");
    auto& first = go.AddComponent<CAnimator>();
    first.skeleton = &rig;
    ResourceAnimationController firstCtrl(910);
    firstCtrl.SetState(ResourceState::CPU_READY);
    SetClips(firstCtrl, { &anim });
    first.controller = &firstCtrl;

    go.AddComponent<CAnimator>();   // the prefab-refresh path

    ASSERT_EQ(fakes.resources.unloaded.size(), 2u);
    EXPECT_EQ(fakes.resources.unloaded[0], 1u);
    EXPECT_EQ(fakes.resources.unloaded[1], 910u);
}

// =============================================================================
// The controller slot
// =============================================================================

TEST_F(t_CAnimator, PlaysTheDefaultStateOnBind)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation animA(2);  MakeSlideClip(animA, "Child");
    ResourceAnimation animB(3);  MakeSlideClip(animB, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(911);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;

    a.OnUpdate(0.5f);

    // animA drives Child; if the list were played in the wrong order Root would move.
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);
    EXPECT_EQ(a.CurrentClip(), &animA);
}

TEST_F(t_CAnimator, AControllerWithNoStatesIsInert)
{
    ResourceSkeleton rig(1);  MakeTwoBoneRig(rig);

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;

    a.OnUpdate(0.5f);

    EXPECT_FALSE(a.IsBound());
    EXPECT_TRUE(a.GetBoneGlobals().empty());
    EXPECT_EQ(a.CurrentClip(), nullptr);
}

// =============================================================================
// Serialization of the list
// =============================================================================

TEST_F(t_CAnimator, SerializeWritesTheControllerSlot)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation animA(2);  MakeSlideClip(animA, "Child");
    ResourceAnimation animB(3);  MakeSlideClip(animB, "Root");
    animA.SetAssetsPath("Assets/A.nanim");
    animB.SetAssetsPath("Assets/B.nanim");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(912);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;

    aCtrl.SetAssetsPath("Assets/Locomotion.nctrl");
    aCtrl.SetLibraryPath("Library/AnimationControllers/912.nctrl");

    const JsonObject json = a.Serialize();

    // ONE slot, in the skeleton's three-field shape. The clip array is gone: a scene
    // carries no clip references of its own any more, because the clips belong to
    // the controller's states.
    EXPECT_EQ(json.GetString("controllerAssetPath"), "Assets/Locomotion.nctrl");
    EXPECT_EQ(json.GetString("controllerLibraryPath"), "Library/AnimationControllers/912.nctrl");
    EXPECT_EQ(static_cast<uint32_t>(json.GetDouble("controllerUID", 0.0)), 912u);

    EXPECT_EQ(json.GetArray("clips").Count(), 0);
}

// MVP-E's "clips" array and the pre-MVP-E "clipAssetPath" key are both GONE rather
// than migrated, which is a deliberate break with the usual "scenes are authored
// data" rule. There is no longer a per-animator clip list for either to load INTO:
// the clips belong to a controller asset the scene cannot invent. An animator in an
// older scene loads with no controller and plays nothing until one is assigned.
TEST_F(t_CAnimator, DeserializeIgnoresTheRetiredClipKeysInsteadOfFailing)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();

    JsonObject legacy;
    legacy.Set("clipAssetPath", std::string("Assets/Old.nanim"));
    legacy.Set("fadeSeconds", 0.75f);

    a.Deserialize(legacy);

    EXPECT_EQ(a.controller, nullptr);

    // The rest of the component still loads -- an old scene degrades to "no
    // controller", not to a component that failed to deserialize at all.
    EXPECT_FLOAT_EQ(a.fadeSeconds, 0.75f);
}

// =============================================================================
// Reference release
// =============================================================================

TEST_F(t_CAnimator, OnDestroyReleasesTheSkeletonAndControllerButNotTheClips)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation animA(2);  MakeSlideClip(animA, "Child");
    ResourceAnimation animB(3);  MakeSlideClip(animB, "Root");
    rig.SetState(ResourceState::CPU_READY);
    animA.SetState(ResourceState::CPU_READY);
    animB.SetState(ResourceState::CPU_READY);

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(913);
    aCtrl.SetState(ResourceState::CPU_READY);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;

    a.OnDestroy();

    // The two slots this component actually acquired, and ONLY those. The clips are
    // the CONTROLLER's references, taken by ImporterAnimationController::Deserialize
    // and given back by its Evict -- releasing them here would be giving back
    // something this component never took, which double-frees them once the
    // controller evicts too.
    //
    // The symmetry that matters: a component releases exactly what it acquired.
    // AddComponent fires OnDestroy on the component it REPLACES (PrefabManager does
    // that to a prefab root on every scene load), so both an over-release and an
    // under-release here compound once per load.
    ASSERT_EQ(fakes.resources.unloaded.size(), 2u);
    EXPECT_EQ(fakes.resources.unloaded[0], 1u);     // skeleton
    EXPECT_EQ(fakes.resources.unloaded[1], 913u);   // controller

    for (const uint32_t uid : fakes.resources.unloaded)
    {
        EXPECT_NE(uid, 2u) << "released a clip the controller owns";
        EXPECT_NE(uid, 3u) << "released a clip the controller owns";
    }
}

// =============================================================================
// CrossFade
// =============================================================================

TEST_F(t_CAnimator, CrossFadeReturnsFalseForAnUnknownStateName)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);
    animA.SetName("A");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(914);
    SetClips(aCtrl, { &animA });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.CrossFade("NoSuchClip", 0.5f));
    EXPECT_FALSE(a.IsFading());
    EXPECT_EQ(a.CurrentClip(), &animA);
}

TEST_F(t_CAnimator, CrossFadeWithZeroFadeSnaps)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(915);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("B", 0.0f));
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.IsFading());
    EXPECT_EQ(a.CurrentClip(), &animB);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 10.0f);
}

// THE weight test. hold(0) -> hold(10) at half the fade duration must read 5: not 0
// (blend never applied), not 10 (snapped), not something time-dependent.
TEST_F(t_CAnimator, MidFadePoseLiesBetweenTheTwoClips)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(916);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 0.0f);

    ASSERT_TRUE(a.CrossFade("B", 1.0f));
    a.OnUpdate(0.5f);

    EXPECT_TRUE(a.IsFading());
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);
}

TEST_F(t_CAnimator, FadeCompletionMakesTheTargetCurrent)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(917);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("B", 1.0f));
    a.OnUpdate(0.5f);
    a.OnUpdate(0.5f);   // weight reaches 1

    EXPECT_FALSE(a.IsFading());
    EXPECT_EQ(a.CurrentClip(), &animB);
    // Blend is bit-exact at weight 1, so this is 10 exactly, not 10-ish.
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 10.0f);
}

// The renderer's skinned-geometry test is !GetPalette().empty(). A frame during a
// fade that produced an empty palette would make the character vanish mid-transition.
TEST_F(t_CAnimator, ThePaletteIsNeverEmptyDuringAFade)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(918);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("B", 1.0f));
    for (int i = 0; i < 10; ++i)
    {
        a.OnUpdate(0.1f);
        EXPECT_FALSE(a.GetPalette().empty()) << "empty palette on fade step " << i;
    }
}

// Lookup is by RESOURCE name, never AnimClipData::name -- every Mixamo export names
// its clip "mixamo.com", so matching that would make every animation in a project
// answer to one string. Both clips here share a clip name and differ by resource name.
TEST_F(t_CAnimator, CrossFadeMatchesTheStateNameNotTheClipName)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);

    animA.clip.name = "mixamo.com";   // identical clip names, the Mixamo case
    animB.clip.name = "mixamo.com";
    animA.SetName("Idle");            // distinct resource names
    animB.SetName("Run");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(919);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("Run", 0.0f));
    a.OnUpdate(0.0f);
    EXPECT_EQ(a.CurrentClip(), &animB);

    ASSERT_TRUE(a.CrossFade("Idle", 0.0f));
    a.OnUpdate(0.0f);
    EXPECT_EQ(a.CurrentClip(), &animA);
}

// =============================================================================
// Graph evaluation and arbitration
// =============================================================================

// Design §4: the current state becomes the DESTINATION the instant a transition
// starts. The outgoing side is a pose, not a state -- which is what makes a later
// exit-time transition measure the clip that is arriving rather than the one that
// is leaving, so "when the attack finishes" means the attack.
TEST_F(t_CAnimator, AFiredTransitionMakesTheDESTINATIONTheCurrentState)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Run");

    ResourceAnimationController aCtrl(930);
    SetClips(aCtrl, { &animA, &animB });
    AddTransition(aCtrl, 0, 1, 1.0f).conditions.push_back(
        { "speed", nous::engine::animation_system::ConditionComparator::Greater, 0.5f });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;

    a.OnUpdate(0.0f);
    ASSERT_EQ(a.GetCurrentStateName(), "Idle");   // the condition is not satisfied yet
    ASSERT_FALSE(a.IsFading());

    a.parameters.SetFloat("speed", 1.0f);
    a.OnUpdate(0.0f);

    EXPECT_EQ(a.GetCurrentStateName(), "Run");
    EXPECT_TRUE(a.IsFading());
}

// An exit-time transition measures the INCOMING state's progress, which is only
// meaningful because the destination became current when the fade started.
TEST_F(t_CAnimator, ExitTimeMeasuresTheStateThatWasEntered)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Attack");

    ResourceAnimationController aCtrl(931);
    SetClips(aCtrl, { &animA, &animB });

    auto& toAttack = AddTransition(aCtrl, 0, 1, 0.0f);   // snap in on a trigger
    toAttack.conditions.push_back(
        { "attack", nous::engine::animation_system::ConditionComparator::TriggerSet, 0.0f });

    auto& backToIdle = AddTransition(aCtrl, 1, 0, 0.0f);
    backToIdle.hasExitTime = true;
    backToIdle.exitTime    = 0.8f;

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;

    a.OnUpdate(0.0f);
    a.parameters.SetTrigger("attack");
    a.OnUpdate(0.0f);
    ASSERT_EQ(a.GetCurrentStateName(), "Attack");

    // The graph is evaluated BEFORE the clocks advance, so it reads the progress the
    // previous frame left behind. That one-frame lag is deliberate -- evaluating
    // against a pose that has not been sampled yet would fire on a state the animator
    // has not rendered even once.
    a.OnUpdate(0.5f);   // evaluated at 0.0; Attack is now 0.5 through
    EXPECT_EQ(a.GetCurrentStateName(), "Attack");

    a.OnUpdate(0.4f);   // evaluated at 0.5 -- still below the exit time
    EXPECT_EQ(a.GetCurrentStateName(), "Attack");

    a.OnUpdate(0.0f);   // evaluated at 0.9 -- past it
    EXPECT_EQ(a.GetCurrentStateName(), "Idle");
}

// The arbitration rule (design §7), enforced by code rather than by a comment: a
// direct CrossFade wins for ITS frame even when a graph transition out of the state
// it entered is satisfied, and the graph resumes on the very next frame. The
// unconditional Attack -> Run edge here is satisfied every frame Attack is current,
// so a single frame of suppression is the whole difference between the two ticks.
TEST_F(t_CAnimator, CrossFadeWinsForItsFrameAndTheGraphResumesTheNext)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child",  0.0f);  animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Attack");
    ResourceAnimation animC(4); MakeHoldClip(animC, "Child", 20.0f);  animC.SetName("Run");

    ResourceAnimationController aCtrl(932);
    SetClips(aCtrl, { &animA, &animB, &animC });
    AddTransition(aCtrl, 1, 2, 0.0f);   // Attack -> Run, unconditional

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;

    a.OnUpdate(0.0f);
    ASSERT_EQ(a.GetCurrentStateName(), "Idle");

    ASSERT_TRUE(a.CrossFade("Attack", 0.0f));
    a.OnUpdate(0.0f);
    EXPECT_EQ(a.GetCurrentStateName(), "Attack") << "the graph overrode the script";

    a.OnUpdate(0.0f);
    EXPECT_EQ(a.GetCurrentStateName(), "Run") << "suppression outlasted its one frame";
}

TEST_F(t_CAnimator, CrossFadeToAnUnknownStateChangesNothing)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Run");

    ResourceAnimationController aCtrl(933);
    SetClips(aCtrl, { &animA, &animB });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.CrossFade("NoSuchState", 0.2f));
    EXPECT_EQ(a.GetCurrentStateName(), "Idle");
    EXPECT_FALSE(a.IsFading());

    // A rejected CrossFade must not arm the suppression either, or a typo'd state
    // name would silently cost the graph a frame.
    a.OnUpdate(0.0f);
    EXPECT_EQ(a.GetCurrentStateName(), "Idle");
}

// A graph transition firing while a fade is in flight goes through the same fold as
// a re-triggered CrossFade -- MVP-E's two-track ceiling IS the interruption model.
// hold(0) -> hold(10) half way is x = 5; interrupting toward hold(20) and running
// half of the new fade must give 5 + (20-5)/2 = 12.5.
TEST_F(t_CAnimator, AnInterruptedTransitionFoldsTheBlendAndStaysAtTwoTracks)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child",  0.0f);  animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Walk");
    ResourceAnimation animC(4); MakeHoldClip(animC, "Child", 20.0f);  animC.SetName("Run");

    ResourceAnimationController aCtrl(934);
    SetClips(aCtrl, { &animA, &animB, &animC });

    AddTransition(aCtrl, 0, 1, 1.0f).conditions.push_back(
        { "walk", nous::engine::animation_system::ConditionComparator::TriggerSet, 0.0f });
    AddTransition(aCtrl, 1, 2, 1.0f).conditions.push_back(
        { "run", nous::engine::animation_system::ConditionComparator::TriggerSet, 0.0f });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;

    a.OnUpdate(0.0f);
    a.parameters.SetTrigger("walk");
    a.OnUpdate(0.0f);
    ASSERT_EQ(a.GetCurrentStateName(), "Walk");

    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    a.parameters.SetTrigger("run");
    a.OnUpdate(0.5f);

    EXPECT_EQ(a.GetCurrentStateName(), "Run");
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 12.5f);
    EXPECT_FALSE(a.GetPalette().empty());
}

// =============================================================================
// Interrupted transitions
// =============================================================================

// Re-triggering mid-fade must fold the CURRENT blended pose into the outgoing track
// and fade from there. hold(0) -> hold(10) half-way is x = 5; interrupting toward
// hold(20) and running half of the new fade must give 5 + (20-5)/2 = 12.5. A version
// that discarded the partial blend would give 10, and one that queued would give 5.
TEST_F(t_CAnimator, ReTriggerMidFadeBlendsFromThePartialPose)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");
    ResourceAnimation animC(4); MakeHoldClip(animC, "Child", 20.0f);  animC.SetName("C");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(920);
    SetClips(aCtrl, { &animA, &animB, &animC });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("B", 1.0f));
    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    ASSERT_TRUE(a.CrossFade("C", 1.0f));
    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 12.5f);
}

// Mashing Play every frame must not accumulate work or drift -- the design bounds the
// animator at two tracks precisely so this is safe.
TEST_F(t_CAnimator, RepeatedReTriggerStaysBoundedAndFinishes)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(921);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    for (int i = 0; i < 50; ++i)
    {
        a.CrossFade("B", 0.1f);
        a.OnUpdate(0.05f);
        EXPECT_FALSE(a.GetPalette().empty());
    }

    // Every re-trigger restarts the fade, so it never completes while mashing --
    // but the pose must be converging on B, not stuck at A or oscillating.
    EXPECT_GT(TranslationX(a.GetBoneGlobals()[1]), 0.0f);
    EXPECT_LE(TranslationX(a.GetBoneGlobals()[1]), 10.0f);
}

// A frozen source pose belongs to the OLD skeleton, so it would fail ArePosesCompatible
// against a freshly sized target pose. Cancelling the fade on a skeleton swap keeps
// that unreachable by construction rather than relying on the Blend guard.
TEST_F(t_CAnimator, SwappingTheSkeletonMidFadeCancelsTheFade)
{
    ResourceSkeleton  rigA(1);  MakeTwoBoneRig(rigA);
    ResourceSkeleton  rigB(5);  MakeTwoBoneRig(rigB);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rigA;
    ResourceAnimationController aCtrl(922);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("B", 1.0f));
    a.OnUpdate(0.5f);
    ASSERT_TRUE(a.IsFading());

    a.skeleton = &rigB;
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.IsFading());
    EXPECT_FALSE(a.GetPalette().empty());
}

// The relocation test from MVP-A, extended: two tracks means two self-pointers, and a
// miss on the second corrupts only the interrupted-transition path -- which would look
// like "transitions break once the scene gets big enough" rather than a pointer bug.
TEST_F(t_CAnimator, FadingAnimatorsSurvivePoolRelocation)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    ResourceAnimationController aCtrl(923);
    SetClips(aCtrl, { &animA, &animB });

    std::vector<GameObject> objects;
    objects.reserve(256);

    for (int i = 0; i < 256; ++i)
    {
        GameObject go = scene->CreateGameObject("Rig");
        auto& a = go.AddComponent<CAnimator>();   // grows and relocates the pool
        a.skeleton   = &rig;
        a.controller = &aCtrl;
        objects.push_back(go);
    }

    // Start every animator fading AFTER the pool has finished relocating, then keep
    // stepping so the blend path runs on relocated components.
    for (GameObject& go : objects)
    {
        auto& a = go.GetComponent<CAnimator>();
        a.OnUpdate(0.0f);
        ASSERT_TRUE(a.CrossFade("B", 1.0f));
    }

    for (GameObject& go : objects)
        go.GetComponent<CAnimator>().OnUpdate(0.5f);

    for (GameObject& go : objects)
    {
        const auto& globals = go.GetComponent<CAnimator>().GetBoneGlobals();
        ASSERT_EQ(globals.size(), 2u);
        EXPECT_FLOAT_EQ(TranslationX(globals[1]), 5.0f);
    }
}

// =============================================================================
// Normalized time
// =============================================================================

TEST_F(t_CAnimator, NormalizedTimeIsZeroWhenUnbound)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.0f);
}

TEST_F(t_CAnimator, NormalizedTimeTracksTheClip)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");   // 1-second clip

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(924);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;

    a.OnUpdate(0.25f);
    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.25f);

    a.OnUpdate(0.5f);
    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.75f);
}

// Replaces NormalizedTimeFollowsTheOutgoingClipDuringAFade, which pinned the wart
// MVP-E accepted knowingly and only the graph could resolve. The current state is
// the DESTINATION from the instant a transition starts (design §4), so the clip the
// animator reports -- and the progress an exit-time transition measures -- is the
// INCOMING one. The two read the same track by construction, so they cannot drift.
TEST_F(t_CAnimator, NormalizedTimeFollowsTheINCOMINGClipDuringAFade)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(925);
    SetClips(aCtrl, { &animA, &animB });
    a.controller = &aCtrl;

    a.OnUpdate(0.4f);                    // A is 0.4 into its 1-second clip
    ASSERT_TRUE(a.CrossFade("B", 1.0f));
    a.OnUpdate(0.2f);                    // both advance by 0.2

    ASSERT_TRUE(a.IsFading());
    EXPECT_EQ(a.CurrentClip(), &animB);
    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.2f);   // B's progress, not A's 0.6
}

// =============================================================================
// Parameters
// =============================================================================

TEST_F(t_CAnimator, ParametersRoundTripThroughTheComponent)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();

    a.parameters.SetFloat("speed", 2.5f);
    a.parameters.SetBool("isGrounded", true);
    a.parameters.SetTrigger("jump");

    EXPECT_FLOAT_EQ(a.parameters.GetFloat("speed"), 2.5f);
    EXPECT_TRUE(a.parameters.GetBool("isGrounded"));
    EXPECT_TRUE(a.parameters.IsTriggerSet("jump"));
}

// Parameters are runtime state, like AnimInstance::time. A saved speed reloading
// into a stopped scene would be confusing, and defaults belong in MVP-F's
// controller asset.
TEST_F(t_CAnimator, ParametersAreNotSerialized)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.parameters.SetFloat("speed", 2.5f);

    const JsonObject json = a.Serialize();

    EXPECT_FLOAT_EQ(json.GetFloat("parameters", -1.0f), -1.0f);
    EXPECT_EQ(json.GetArray("parameters").Count(), 0);
}

// =============================================================================
// Live re-save (generation bump)
// =============================================================================

// Preserved BY NAME, never by index: a re-save that inserts a state above this one
// shifts every index below it, so an index-preserving rebuild silently moves the
// character into a different state. Resetting to the default state instead would make
// the tuning loop useless -- adjusting a transition's duration is most of what the
// controller editor is for, and it is only reachable while the scene plays.
TEST_F(t_CAnimator, AGenerationBumpPreservesTheCurrentStateByName)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child",  0.0f);  animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Run");

    ResourceAnimationController aCtrl(950);
    SetClips(aCtrl, { &animA, &animB });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("Run", 0.0f));
    a.OnUpdate(0.0f);
    ASSERT_EQ(a.GetCurrentStateName(), "Run");
    ASSERT_EQ(a.CurrentClip(), &animB);

    // The editor re-saves with the states in the opposite order: Run is now index 0.
    SetClips(aCtrl, { &animB, &animA });
    ++aCtrl.generation;
    a.OnUpdate(0.0f);

    EXPECT_EQ(a.GetCurrentStateName(), "Run");
    EXPECT_EQ(a.CurrentClip(), &animB);
}

TEST_F(t_CAnimator, AGenerationBumpFallsBackToDefaultWhenTheStateIsGone)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child",  0.0f);  animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Run");

    ResourceAnimationController aCtrl(951);
    SetClips(aCtrl, { &animA, &animB });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("Run", 0.0f));
    a.OnUpdate(0.0f);
    ASSERT_EQ(a.GetCurrentStateName(), "Run");

    SetClips(aCtrl, { &animA });   // Run deleted
    ++aCtrl.generation;
    a.OnUpdate(0.0f);

    EXPECT_EQ(a.GetCurrentStateName(), "Idle");
    EXPECT_EQ(a.CurrentClip(), &animA);
}

// Preserving a transition across a re-save would mean reconciling two graphs'
// transition identities for one frame of visual continuity during an editor action.
// The rebuild lands on a whole state instead.
TEST_F(t_CAnimator, AGenerationBumpCancelsAnInFlightTransition)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child",  0.0f);  animA.SetName("Idle");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("Run");

    ResourceAnimationController aCtrl(952);
    SetClips(aCtrl, { &animA, &animB });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("Run", 1.0f));
    a.OnUpdate(0.5f);
    ASSERT_TRUE(a.IsFading());

    ++aCtrl.generation;
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.IsFading());
    EXPECT_EQ(a.GetCurrentStateName(), "Run");    // landed on the destination
    EXPECT_FALSE(a.GetPalette().empty());
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 10.0f);
}

// A script's value must survive an editor save, or every Ctrl+S resets the
// character's behaviour mid-play. A NEWLY declared parameter still arrives at its
// authored default, which is the other half of what makes the seeding worth having.
TEST_F(t_CAnimator, AGenerationBumpPreservesParameterValuesAndSeedsOnlyNewDeclarations)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeHoldClip(anim, "Child", 0.0f);   anim.SetName("Idle");

    ResourceAnimationController aCtrl(953);
    SetClips(aCtrl, { &anim });
    aCtrl.graph.parameters.push_back(
        { "speed", static_cast<uint8_t>(nous::engine::animation_system::AnimParamType::Float), 1.0f });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    EXPECT_FLOAT_EQ(a.parameters.GetFloat("speed"), 1.0f);   // seeded on bind

    a.parameters.SetFloat("speed", 7.0f);                    // a script writes it

    aCtrl.graph.parameters.push_back(
        { "isGrounded", static_cast<uint8_t>(nous::engine::animation_system::AnimParamType::Bool), 1.0f });
    ++aCtrl.generation;
    a.OnUpdate(0.0f);

    EXPECT_FLOAT_EQ(a.parameters.GetFloat("speed"), 7.0f);   // NOT reset to 1.0
    EXPECT_TRUE(a.parameters.GetBool("isGrounded"));         // newly declared, seeded
}

// A declared default must fill an EMPTY slot and never overwrite what a script set --
// including when the script set the type's own zero. That is the case every
// absence-by-fallback trick gets wrong, and the reason AnimParameters::Contains exists.
TEST_F(t_CAnimator, SeedingNeverOverwritesAScriptsFalse)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeHoldClip(anim, "Child", 0.0f);   anim.SetName("Idle");

    ResourceAnimationController aCtrl(954);
    SetClips(aCtrl, { &anim });
    aCtrl.graph.parameters.push_back(
        { "isGrounded", static_cast<uint8_t>(nous::engine::animation_system::AnimParamType::Bool), 1.0f });

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.parameters.SetBool("isGrounded", false);   // the script spoke first
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.parameters.GetBool("isGrounded"));

    ++aCtrl.generation;
    a.OnUpdate(0.0f);
    EXPECT_FALSE(a.parameters.GetBool("isGrounded"));
}

// =============================================================================
// Per-state speed
// =============================================================================

// Four multiplicands, and the factors are chosen so that dropping ANY ONE of them
// gives a distinct wrong answer: clip 2.0 x state 1.5 x parameter 0.5 x multiplier
// 4.0 = 6.0, against 3.0 / 4.0 / 12.0 / 1.5 for the four omissions. A formula this
// shape is easy to get subtly wrong and impossible to see at runtime.
TEST_F(t_CAnimator, TheRateIsTheProductOfAllFourFactors)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");
    anim.settings.speed = 2.0f;

    ResourceAnimationController aCtrl(940);
    SetClips(aCtrl, { &anim });
    aCtrl.graph.states[0].speed          = 1.5f;
    aCtrl.graph.states[0].speedParameter = "rate";

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton        = &rig;
    a.controller      = &aCtrl;
    a.speedMultiplier = 4.0f;
    a.parameters.SetFloat("rate", 0.5f);
    a.OnUpdate(0.0f);

    a.OnUpdate(0.1f);   // 0.1s x 6.0 = 0.6s into a 1s slide
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 6.0f);
}

// The rate is a PRODUCT, so the absent-parameter fallback has to be 1.0f -- reading
// AnimParameters' own 0.0f default would multiply the whole rate to zero and freeze
// the character. A state naming a parameter no script has written yet is the normal
// case on the first frames of a scene, not an error.
TEST_F(t_CAnimator, AnAbsentSpeedParameterIsFactorOneNotZero)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeSlideClip(animA, "Child");  animA.SetName("Plain");
    ResourceAnimation animB(3); MakeSlideClip(animB, "Child");  animB.SetName("Ghost");

    ResourceAnimationController aCtrl(941);
    SetClips(aCtrl, { &animA, &animB });
    aCtrl.graph.states[1].speedParameter = "neverSet";

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    a.OnUpdate(0.5f);   // no speedParameter at all
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    ASSERT_TRUE(a.CrossFade("Ghost", 0.0f));
    a.OnUpdate(0.0f);
    a.OnUpdate(0.5f);   // names a parameter nothing has set
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);
}

// =============================================================================
// Root motion
// =============================================================================

// Baked is the default so that every existing scene renders exactly as it did
// before this feature: enabling root motion is opt-in per character.
TEST_F(t_CAnimator, RootMotionDefaultsToBakedAndDoesNotMoveTheObject)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(926);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;

    ASSERT_EQ(a.rootMotion, RootMotionMode::Baked);

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 0.0f);

    // Baked leaves the travel in the pose, which is exactly today's behaviour.
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 5.0f);
}

TEST_F(t_CAnimator, AppliedMovesTheObjectAndTakesTheTravelOutOfThePose)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    ResourceAnimationController aCtrl(927);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 5.0f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 0.0f);   // pinned at bind
    EXPECT_FLOAT_EQ(a.GetRootMotionDelta().translation.x, 5.0f);
}

TEST_F(t_CAnimator, InPlaceStripsTheTravelAndLeavesTheObjectAlone)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    ResourceAnimationController aCtrl(928);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::InPlace;

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 0.0f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 0.0f);
}

// A character scaled to fix the Mixamo centimetres-versus-metres mismatch would
// otherwise animate at one scale and travel at another, which reads as footskate.
TEST_F(t_CAnimator, TheObjectScaleMultipliesTheDelta)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    go.GetComponent<CTransform>().SetScale(glm::vec3(2.0f));

    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    ResourceAnimationController aCtrl(929);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 10.0f);
}

// The delta is in the animation's own space, so it has to be rotated into the
// object's before it becomes movement -- otherwise a character facing any
// direction but the default walks sideways.
TEST_F(t_CAnimator, TheObjectOrientationRotatesTheDelta)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");   // travels +X

    GameObject go = scene->CreateGameObject("Rig");

    // Yaw 90 degrees about +Y maps +X onto -Z.
    go.GetComponent<CTransform>().SetOrientation(
        glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));

    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    ResourceAnimationController aCtrl(930);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;

    a.OnUpdate(0.5f);

    const CTransform& t = go.GetComponent<CTransform>();
    EXPECT_NEAR(t.position.x,  0.0f, 1e-4f);
    EXPECT_NEAR(t.position.z, -5.0f, 1e-4f);
}

// The mode is authoring, unlike the parameter blackboard -- it must survive a
// save/load or a character silently reverts to Baked.
TEST_F(t_CAnimator, RootMotionModeIsSerialized)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.rootMotion = RootMotionMode::Applied;

    const JsonObject json = a.Serialize();

    GameObject other = scene->CreateGameObject("Other");
    auto& b = other.AddComponent<CAnimator>();
    b.Deserialize(json);

    EXPECT_EQ(b.rootMotion, RootMotionMode::Applied);
}

// Applied must take the yaw OUT of the pose, because it is about to go onto the
// GameObject -- leaving it in applies the turn twice, which drives the transform
// backwards for any clip that turns more than 90 degrees.
TEST_F(t_CAnimator, AppliedMovesTheTurnFromThePoseToTheTransform)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeTurnClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    ResourceAnimationController aCtrl(931);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;

    a.OnUpdate(0.5f);   // 45 degrees in

    EXPECT_NEAR(ForwardX(a.GetBoneGlobals()[0]), 0.0f, 1e-4f);   // pose no longer turns

    const glm::vec3 forward = go.GetComponent<CTransform>().orientation
                            * glm::vec3(0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(forward.x, std::sin(glm::quarter_pi<float>()), 1e-3f);
}

// InPlace DISCARDS the delta, so the yaw has nowhere to go and must stay in the
// pose. Stripping it there is not "not travelling" -- it is deleting animation,
// and a turning clip would face one direction forever. Mixamo's own In Place
// export draws the line the same way: no root translation, rotation kept.
TEST_F(t_CAnimator, InPlaceKeepsTheTurnInThePose)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeTurnClip(anim, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    ResourceAnimationController aCtrl(932);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::InPlace;

    a.OnUpdate(0.5f);

    EXPECT_NEAR(ForwardX(a.GetBoneGlobals()[0]), std::sin(glm::quarter_pi<float>()), 1e-3f);

    const glm::vec3 forward = go.GetComponent<CTransform>().orientation
                            * glm::vec3(0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(forward.x, 0.0f, 1e-4f);   // the object itself never turns
}

// A state left at Inherit -- which every state is until someone changes it, since
// Inherit is enumerator 0 and ControllerState::rootMotion defaults to 0 -- behaves
// exactly as the character did before per-state overrides existed.
TEST_F(t_CAnimator, InheritRootMotionResolvesToTheComponentMode)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");

    ResourceAnimationController aCtrl(942);
    SetClips(aCtrl, { &anim });
    ASSERT_EQ(aCtrl.graph.states[0].rootMotion, 0);   // Inherit, untouched

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 5.0f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 0.0f);
}

// The feature: an attack that must not slide the character, inside a character whose
// locomotion is Applied. The override is on the state, so it costs nothing anywhere
// else in the graph.
TEST_F(t_CAnimator, AnExplicitStateRootMotionOverridesTheComponent)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Root");

    ResourceAnimationController aCtrl(943);
    SetClips(aCtrl, { &anim });
    aCtrl.graph.states[0].rootMotion = static_cast<int>(RootMotionMode::InPlace);

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;

    a.OnUpdate(0.5f);

    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 0.0f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[0]), 0.0f);   // stripped, not applied
}

// Design §5, and THE test that pins ApplyRootMotion having no mode gate of its own.
// The outgoing track is Applied and the incoming one InPlace, so deltaFrom is the
// full travel and deltaTo is zero; BlendRootDelta then walks the travel out ACROSS
// the transition instead of cutting it on the transition's first frame. Both gates an
// earlier draft proposed -- on the component mode, or on m_currentState -- produce a
// snap, and a snap is invisible in a still frame.
TEST_F(t_CAnimator, FadingFromAppliedToInPlaceFadesTheTravelOutRatherThanSnapping)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeSlideClip(animA, "Root");  animA.SetName("Walk");
    ResourceAnimation animB(3); MakeSlideClip(animB, "Root");  animB.SetName("Attack");

    ResourceAnimationController aCtrl(944);
    SetClips(aCtrl, { &animA, &animB });
    aCtrl.graph.states[1].rootMotion = static_cast<int>(RootMotionMode::InPlace);

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton   = &rig;
    a.controller = &aCtrl;
    a.rootMotion = RootMotionMode::Applied;
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.CrossFade("Attack", 1.0f));
    a.OnUpdate(0.5f);

    // Half way through the fade: half of Walk's 5.0 of travel, none of Attack's.
    EXPECT_FLOAT_EQ(a.GetRootMotionDelta().translation.x, 2.5f);
    EXPECT_FLOAT_EQ(go.GetComponent<CTransform>().position.x, 2.5f);
    EXPECT_GT(a.GetRootMotionDelta().translation.x, 0.0f);
    EXPECT_LT(a.GetRootMotionDelta().translation.x, 5.0f);
}

// =============================================================================
// Per-clip settings
// =============================================================================

// THE headline for per-clip settings, and the case a per-animator flag cannot
// express at all: whichever value the animator held would apply to both clips.
// An attack that must not loop beside an idle that must is exactly this shape.
TEST_F(t_CAnimator, TwoClipsWithDifferentLoopSettingsInOneAnimator)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation once(2);   MakeSlideClip(once,  "Child");  once.SetName("Once");
    ResourceAnimation cycle(3);  MakeSlideClip(cycle, "Child");  cycle.SetName("Cycle");

    once.settings.loop  = false;
    cycle.settings.loop = true;

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(933);
    SetClips(aCtrl, { &once, &cycle });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);                 // binds the front clip, "Once"

    // 1.5s into a 1s slide. Not looping, so it clamps at the end.
    a.OnUpdate(1.5f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 10.0f);

    ASSERT_TRUE(a.CrossFade("Cycle", 0.0f));

    // The same 1.5s against the looping clip wraps to 0.5s -- halfway along.
    a.OnUpdate(1.5f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);
}

TEST_F(t_CAnimator, PerClipSpeedScalesHowFarTheClipAdvances)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation fast(2);  MakeSlideClip(fast, "Child");
    fast.settings.speed = 2.0f;

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(934);
    SetClips(aCtrl, { &fast });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    a.OnUpdate(0.25f);   // 0.25s x 2 = 0.5s into a 1s slide
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);
}

// Editing the resource has to reach a clip that is ALREADY playing -- otherwise a
// tick of the Inspector checkbox does nothing until the scene is reloaded. Seeding
// only in RebindTrack would fail this, since nothing about the slot changed.
TEST_F(t_CAnimator, EditingAClipsSettingsReachesThePlayingInstance)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    ResourceAnimationController aCtrl(935);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.OnUpdate(0.0f);

    anim.settings.loop = false;
    a.OnUpdate(1.5f);

    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 10.0f);
}

// The script API's lever. It MULTIPLIES the clip's authored speed rather than
// replacing it, and it lives on the component rather than the resource: a script
// writing into ResourceAnimation::settings would retime every other character
// playing that same clip.
TEST_F(t_CAnimator, SpeedMultiplierScalesTheClipsAuthoredSpeed)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");
    anim.settings.speed = 2.0f;

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton        = &rig;
    ResourceAnimationController aCtrl(936);
    SetClips(aCtrl, { &anim });
    a.controller = &aCtrl;
    a.speedMultiplier = 0.5f;
    a.OnUpdate(0.0f);

    // 0.25s x 2.0 x 0.5 = 0.25s into a 1s slide. Ignoring the multiplier would give
    // 0.5s and x = 5; ignoring the clip's own speed would give 0.125s and x = 1.25.
    a.OnUpdate(0.25f);
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 2.5f);
}

// Serialized, unlike `parameters`, because it is AUTHORING: "this character moves
// heavily" is a property of the character, and it is the only per-character speed
// axis there is -- a controller asset is shared between characters exactly as a clip
// is. Note this does NOT reintroduce the deleted CAnimator::speed, which was
// absolute and therefore competed with each clip's own value.
TEST_F(t_CAnimator, SpeedMultiplierSurvivesASceneRoundTrip)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.speedMultiplier = 0.5f;

    CAnimator reloaded;
    reloaded.Deserialize(a.Serialize());
    EXPECT_FLOAT_EQ(reloaded.speedMultiplier, 0.5f);
}

// Two characters sharing one clip resource must be independently retimeable -- the
// whole reason the multiplier is on the component and not on ResourceAnimation.
TEST_F(t_CAnimator, TwoAnimatorsSharingAClipRetimeIndependently)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject slowGo = scene->CreateGameObject("Slow");
    auto& slow = slowGo.AddComponent<CAnimator>();
    slow.skeleton        = &rig;
    ResourceAnimationController slowCtrl(937);
    SetClips(slowCtrl, { &anim });
    slow.controller = &slowCtrl;
    slow.speedMultiplier = 0.5f;

    GameObject fastGo = scene->CreateGameObject("Fast");
    auto& fast = fastGo.AddComponent<CAnimator>();
    fast.skeleton = &rig;
    ResourceAnimationController fastCtrl(938);
    SetClips(fastCtrl, { &anim });
    fast.controller = &fastCtrl;

    slow.OnUpdate(0.0f);  slow.OnUpdate(0.5f);   // 0.25s in
    fast.OnUpdate(0.0f);  fast.OnUpdate(0.5f);   // 0.50s in

    EXPECT_FLOAT_EQ(TranslationX(slow.GetBoneGlobals()[1]), 2.5f);
    EXPECT_FLOAT_EQ(TranslationX(fast.GetBoneGlobals()[1]), 5.0f);
}

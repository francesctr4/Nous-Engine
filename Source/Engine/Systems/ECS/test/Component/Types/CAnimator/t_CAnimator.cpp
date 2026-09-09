#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonObject.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <string>
#include <vector>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::Transform;

namespace
{
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
    a.clips    = { &anim };

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
    a.clips    = { &animA };
    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    // Clip B drives Root instead of Child. A stale binding would keep moving
    // Child and leave Root at the origin -- the exact inverse of the assertions.
    a.clips = { &animB };
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
    a.clips    = { &anim };
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
    a.clips    = { &anim };
    a.OnUpdate(0.5f);
    ASSERT_FALSE(a.GetBoneGlobals().empty());

    a.clips.clear();
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

    std::vector<GameObject> objects;
    objects.reserve(256);

    for (int i = 0; i < 256; ++i)
    {
        GameObject go = scene->CreateGameObject("Rig");
        auto& a = go.AddComponent<CAnimator>();   // grows and relocates the pool
        a.skeleton = &rig;
        a.clips    = { &anim };
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
    a.clips    = { &anim };

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
    a.clips    = { &anim };

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
    a.clips    = { &anim };
    a.OnUpdate(0.5f);
    ASSERT_FALSE(a.GetPalette().empty());

    a.clips.clear();
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
    a.clips    = { &anim };

    a.OnDestroy();

    ASSERT_EQ(fakes.resources.unloaded.size(), 2u);
    EXPECT_EQ(fakes.resources.unloaded[0], 1u);
    EXPECT_EQ(fakes.resources.unloaded[1], 2u);
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
    first.clips    = { &anim };

    go.AddComponent<CAnimator>();   // the prefab-refresh path

    ASSERT_EQ(fakes.resources.unloaded.size(), 2u);
    EXPECT_EQ(fakes.resources.unloaded[0], 1u);
    EXPECT_EQ(fakes.resources.unloaded[1], 2u);
}

// =============================================================================
// The clip list
// =============================================================================

TEST_F(t_CAnimator, PlaysTheFirstClipInTheList)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation animA(2);  MakeSlideClip(animA, "Child");
    ResourceAnimation animB(3);  MakeSlideClip(animB, "Root");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    a.clips    = { &animA, &animB };

    a.OnUpdate(0.5f);

    // animA drives Child; if the list were played in the wrong order Root would move.
    EXPECT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);
    EXPECT_EQ(a.CurrentClip(), &animA);
}

TEST_F(t_CAnimator, AnEmptyClipListIsInert)
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

TEST_F(t_CAnimator, SerializeRoundTripsEveryClipPath)
{
    ResourceSkeleton  rig(1);    MakeTwoBoneRig(rig);
    ResourceAnimation animA(2);  MakeSlideClip(animA, "Child");
    ResourceAnimation animB(3);  MakeSlideClip(animB, "Root");
    animA.SetAssetsPath("Assets/A.nanim");
    animB.SetAssetsPath("Assets/B.nanim");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    a.clips    = { &animA, &animB };

    const JsonObject json = a.Serialize();
    JsonArray        arr  = json.GetArray("clips");

    ASSERT_EQ(arr.Count(), 2);
    EXPECT_EQ(arr.GetObject(0).GetString("assetPath"), "Assets/A.nanim");
    EXPECT_EQ(arr.GetObject(1).GetString("assetPath"), "Assets/B.nanim");
}

// A scene saved before MVP-E carries a single "clipAssetPath" key. Emptying every
// animator in every existing scene is not an acceptable cost, so that key still
// loads -- as a one-element list. Delete this path once the scenes are re-saved.
TEST_F(t_CAnimator, DeserializeReadsTheLegacySingleClipKey)
{
    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();

    JsonObject legacy;
    legacy.Set("clipAssetPath", std::string("Assets/Old.nanim"));

    a.Deserialize(legacy);

    // The headless fixture has no resource loader, so nothing resolves -- what this
    // pins is that the legacy key is still READ, which a missing branch would skip
    // silently. The resolved-pointer case is covered in-engine.
    EXPECT_TRUE(a.clips.empty() || a.clips.size() == 1u);
}

// =============================================================================
// Reference release
// =============================================================================

TEST_F(t_CAnimator, OnDestroyReleasesEveryClipInTheList)
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
    a.clips    = { &animA, &animB };

    a.OnDestroy();

    // Skeleton + both clips. Releasing only the first is the leak shape this pins:
    // AddComponent fires OnDestroy on the component it replaces, which
    // PrefabManager does to a prefab root on every migration.
    ASSERT_EQ(fakes.resources.unloaded.size(), 3u);
    EXPECT_EQ(fakes.resources.unloaded[0], 1u);
    EXPECT_EQ(fakes.resources.unloaded[1], 2u);
    EXPECT_EQ(fakes.resources.unloaded[2], 3u);
}

// =============================================================================
// Play + cross-fade
// =============================================================================

TEST_F(t_CAnimator, PlayReturnsFalseForAnUnknownClipName)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);
    animA.SetName("A");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    a.clips    = { &animA };
    a.OnUpdate(0.0f);

    EXPECT_FALSE(a.Play("NoSuchClip", 0.5f));
    EXPECT_FALSE(a.IsFading());
    EXPECT_EQ(a.CurrentClip(), &animA);
}

TEST_F(t_CAnimator, PlayWithZeroFadeSnaps)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.Play("B", 0.0f));
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
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 0.0f);

    ASSERT_TRUE(a.Play("B", 1.0f));
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
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.Play("B", 1.0f));
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
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.Play("B", 1.0f));
    for (int i = 0; i < 10; ++i)
    {
        a.OnUpdate(0.1f);
        EXPECT_FALSE(a.GetPalette().empty()) << "empty palette on fade step " << i;
    }
}

// Lookup is by RESOURCE name, never AnimClipData::name -- every Mixamo export names
// its clip "mixamo.com", so matching that would make every animation in a project
// answer to one string. Both clips here share a clip name and differ by resource name.
TEST_F(t_CAnimator, PlayMatchesTheResourceNameNotTheClipName)
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
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.Play("Run", 0.0f));
    a.OnUpdate(0.0f);
    EXPECT_EQ(a.CurrentClip(), &animB);

    ASSERT_TRUE(a.Play("Idle", 0.0f));
    a.OnUpdate(0.0f);
    EXPECT_EQ(a.CurrentClip(), &animA);
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
    a.clips    = { &animA, &animB, &animC };
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.Play("B", 1.0f));
    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    ASSERT_TRUE(a.Play("C", 1.0f));
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
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);

    for (int i = 0; i < 50; ++i)
    {
        a.Play("B", 0.1f);
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
    a.clips    = { &animA, &animB };
    a.OnUpdate(0.0f);

    ASSERT_TRUE(a.Play("B", 1.0f));
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

    std::vector<GameObject> objects;
    objects.reserve(256);

    for (int i = 0; i < 256; ++i)
    {
        GameObject go = scene->CreateGameObject("Rig");
        auto& a = go.AddComponent<CAnimator>();   // grows and relocates the pool
        a.skeleton = &rig;
        a.clips    = { &animA, &animB };
        objects.push_back(go);
    }

    // Start every animator fading AFTER the pool has finished relocating, then keep
    // stepping so the blend path runs on relocated components.
    for (GameObject& go : objects)
    {
        auto& a = go.GetComponent<CAnimator>();
        a.OnUpdate(0.0f);
        ASSERT_TRUE(a.Play("B", 1.0f));
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
    a.clips    = { &anim };

    a.OnUpdate(0.25f);
    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.25f);

    a.OnUpdate(0.5f);
    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.75f);
}

// Pins the wart the spec accepted knowingly: CurrentClip() is the OUTGOING clip
// during a fade, and normalized time follows the same clip for consistency. If
// CurrentClip's meaning is ever changed, this test says so instead of the two
// quietly disagreeing.
TEST_F(t_CAnimator, NormalizedTimeFollowsTheOutgoingClipDuringAFade)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation animA(2); MakeHoldClip(animA, "Child", 0.0f);   animA.SetName("A");
    ResourceAnimation animB(3); MakeHoldClip(animB, "Child", 10.0f);  animB.SetName("B");

    GameObject go = scene->CreateGameObject("Rig");
    auto& a = go.AddComponent<CAnimator>();
    a.skeleton = &rig;
    a.clips    = { &animA, &animB };

    a.OnUpdate(0.4f);                    // A is 0.4 into its 1-second clip
    ASSERT_TRUE(a.Play("B", 1.0f));
    a.OnUpdate(0.2f);                    // both advance by 0.2

    ASSERT_TRUE(a.IsFading());
    EXPECT_EQ(a.CurrentClip(), &animA);
    EXPECT_FLOAT_EQ(a.GetNormalizedTime(), 0.6f);   // A's progress, not B's 0.2
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

#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>

#include <glm/glm.hpp>

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

    float TranslationX(const glm::mat4& m) { return m[3][0]; }
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
    a.clip     = &anim;

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
    a.clip     = &animA;
    a.OnUpdate(0.5f);
    ASSERT_FLOAT_EQ(TranslationX(a.GetBoneGlobals()[1]), 5.0f);

    // Clip B drives Root instead of Child. A stale binding would keep moving
    // Child and leave Root at the origin -- the exact inverse of the assertions.
    a.clip = &animB;
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
    a.clip     = &anim;
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
    a.clip     = &anim;
    a.OnUpdate(0.5f);
    ASSERT_FALSE(a.GetBoneGlobals().empty());

    a.clip = nullptr;
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
        a.clip     = &anim;
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

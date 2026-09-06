#include <gtest/gtest.h>

#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/Component/Types/CBoneAttachment/CBoneAttachment.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/GameObject.h>
#include <ECS/Scene/Scene.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>

#include <glm/glm.hpp>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::Transform;

namespace
{
    // Two bones: "Root" (index 0, no parent) and "Child" (index 1, parent 0).
    // Bind locals are identity, so any translation appearing in the globals
    // demonstrably came from the clip and not from the bind pose.
    void MakeTwoBoneRig(ResourceSkeleton& rig)
    {
        auto& s = rig.skeleton;
        s.names      = { "Root", "Child" };
        s.parents    = { -1, 0 };
        s.bindLocals = { Transform{}, Transform{} };
        s.offsets    = { glm::mat4(1.0f), glm::mat4(1.0f) };
        s.RebuildLookup();
    }

    // A 1-second clip translating `boneName` from x = 0 to x = endX.
    void MakeSlideClip(ResourceAnimation& anim, const char* boneName, float endX = 10.0f)
    {
        anim.clip.name     = "Slide";
        anim.clip.duration = 1.0f;

        AnimChannel ch;
        ch.boneName  = boneName;
        ch.posTimes  = { 0.0f, 1.0f };
        ch.posValues = { glm::vec3(0.0f), glm::vec3(endX, 0.0f, 0.0f) };

        anim.clip.channels = { ch };
    }

    glm::vec3 TranslationOf(const glm::mat4& m) { return glm::vec3(m[3]); }
}

class t_CBoneAttachment : public ::testing::Test
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
// Serialization
// =============================================================================

TEST_F(t_CBoneAttachment, SerializeRoundTripsTheBoneName)
{
    GameObject go = scene->CreateGameObject("Sword");
    auto& attachment = go.AddComponent<CBoneAttachment>();
    attachment.boneName = "mixamorig:RightHand";

    const JsonObject json = attachment.Serialize();

    GameObject other = scene->CreateGameObject("Other");
    auto& restored = other.AddComponent<CBoneAttachment>();
    restored.Deserialize(json);

    EXPECT_EQ(restored.boneName, "mixamorig:RightHand");
    EXPECT_EQ(json.GetString("type"), "CBoneAttachment");
}

// A name the user cleared must round-trip as cleared, not as the previous value.
TEST_F(t_CBoneAttachment, SerializeRoundTripsAnEmptyBoneName)
{
    GameObject go = scene->CreateGameObject("Sword");
    auto& attachment = go.AddComponent<CBoneAttachment>();

    GameObject other = scene->CreateGameObject("Other");
    auto& restored = other.AddComponent<CBoneAttachment>();
    restored.boneName = "stale";
    restored.Deserialize(attachment.Serialize());

    EXPECT_TRUE(restored.boneName.empty());
}

// Deserialize supplies a new name, so any warning already emitted refers to a bone
// this component no longer names.
TEST_F(t_CBoneAttachment, DeserializeClearsTheWarnOnceFlag)
{
    GameObject go = scene->CreateGameObject("Sword");
    auto& attachment = go.AddComponent<CBoneAttachment>();
    attachment.warnedUnresolved = true;

    attachment.Deserialize(attachment.Serialize());

    EXPECT_FALSE(attachment.warnedUnresolved);
}

// =============================================================================
// ComputeParentWorld -- the seam the Scene recursion and the editor gizmo share
// =============================================================================

// The load-bearing composition: parentWorld * boneGlobal, in that order. The
// character sits at z = 3 and the bone has slid to x = 5, so a swapped order or a
// dropped factor moves the result off (5, 0, 3) in a way the axes distinguish.
TEST_F(t_CBoneAttachment, ComposesParentWorldWithTheBoneGlobal)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    character.GetComponent<CTransform>().SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;
    animator.OnUpdate(0.5f);          // bone "Child" is now at x = 5

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->UpdateWorldMatrices();     // gives the CHARACTER its world matrix

    const glm::vec3 t = TranslationOf(ComputeParentWorld(scene->GetRegistry(), prop.GetEntity()));
    EXPECT_FLOAT_EQ(t.x, 5.0f);
    EXPECT_FLOAT_EQ(t.z, 3.0f);
}

// The animator may be any ancestor, not only the immediate parent -- props are
// commonly grouped under an empty "Equipment" node.
TEST_F(t_CBoneAttachment, WalksPastIntermediateAncestorsToTheAnimator)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;
    animator.OnUpdate(0.5f);

    GameObject group = scene->CreateGameObject("Equipment", &character);
    GameObject prop  = scene->CreateGameObject("Sword", &group);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(ComputeParentWorld(scene->GetRegistry(), prop.GetEntity())).x, 5.0f);
}

// ---- Every failure degrades to the plain parent world ------------------------

TEST_F(t_CBoneAttachment, NoAnimatorAncestorYieldsThePlainParentWorld)
{
    GameObject parent = scene->CreateGameObject("Empty");
    parent.GetComponent<CTransform>().SetPosition(glm::vec3(7.0f, 0.0f, 0.0f));

    GameObject prop = scene->CreateGameObject("Sword", &parent);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(ComputeParentWorld(scene->GetRegistry(), prop.GetEntity())).x, 7.0f);
}

TEST_F(t_CBoneAttachment, UnknownBoneNameYieldsThePlainParentWorld)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    character.GetComponent<CTransform>().SetPosition(glm::vec3(7.0f, 0.0f, 0.0f));
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;
    animator.OnUpdate(0.5f);

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "NoSuchBone";

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(ComputeParentWorld(scene->GetRegistry(), prop.GetEntity())).x, 7.0f);
}

// An animator with no clip never samples, so GetBoneGlobals() is empty. Indexing it
// would read out of bounds; the contract is to fall back instead.
TEST_F(t_CBoneAttachment, UnboundAnimatorYieldsThePlainParentWorld)
{
    ResourceSkeleton rig(1);  MakeTwoBoneRig(rig);

    GameObject character = scene->CreateGameObject("Character");
    character.GetComponent<CTransform>().SetPosition(glm::vec3(7.0f, 0.0f, 0.0f));
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;         // no clip
    animator.OnUpdate(0.5f);

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(ComputeParentWorld(scene->GetRegistry(), prop.GetEntity())).x, 7.0f);
}

TEST_F(t_CBoneAttachment, EmptyBoneNameYieldsThePlainParentWorld)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    character.GetComponent<CTransform>().SetPosition(glm::vec3(7.0f, 0.0f, 0.0f));
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;
    animator.OnUpdate(0.5f);

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>();   // boneName left empty

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(ComputeParentWorld(scene->GetRegistry(), prop.GetEntity())).x, 7.0f);
}

// A root object has no parent at all; identity, not a crash.
TEST_F(t_CBoneAttachment, RootObjectYieldsIdentity)
{
    GameObject prop = scene->CreateGameObject("Sword");
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    const glm::mat4 pw = ComputeParentWorld(scene->GetRegistry(), prop.GetEntity());

    EXPECT_EQ(pw, glm::mat4(1.0f));
}

// An object with no CBoneAttachment at all must be unaffected -- ComputeParentWorld
// replaces the gizmo's plain parent lookup for EVERY object, not just attached ones.
TEST_F(t_CBoneAttachment, ObjectWithoutTheComponentYieldsThePlainParentWorld)
{
    GameObject parent = scene->CreateGameObject("Empty");
    parent.GetComponent<CTransform>().SetPosition(glm::vec3(7.0f, 0.0f, 0.0f));

    GameObject child = scene->CreateGameObject("Child", &parent);

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(ComputeParentWorld(scene->GetRegistry(), child.GetEntity())).x, 7.0f);
}

// =============================================================================
// The Scene::UpdateWorldMatrices hook -- end to end through the real pipeline
//
// Scene::Update ticks CAnimator::OnUpdate (sampling the pose); UpdateWorldMatrices
// then propagates. That is the real per-frame order: ModuleScene calls Update
// during its own Update and UpdateWorldMatrices in PostUpdate.
// =============================================================================

TEST_F(t_CBoneAttachment, AttachedPropFollowsTheAnimatedBone)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->Update(0.5f);
    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(prop.GetComponent<CTransform>().worldMatrix).x, 5.0f);
}

// A prop the user has nudged into the hand must land at bone + offset, not at
// either one alone.
TEST_F(t_CBoneAttachment, TheOffsetComposesOnTopOfTheBone)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";
    prop.GetComponent<CTransform>().SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));

    scene->Update(0.5f);
    scene->UpdateWorldMatrices();

    const glm::vec3 t = TranslationOf(prop.GetComponent<CTransform>().worldMatrix);
    EXPECT_FLOAT_EQ(t.x, 5.0f);
    EXPECT_FLOAT_EQ(t.y, 1.0f);
}

// The recursion continues below an attachment, so a trail effect parented to the
// sword rides along too.
TEST_F(t_CBoneAttachment, GrandchildrenOfAnAttachedPropFollowIt)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";
    prop.GetComponent<CTransform>().SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));

    GameObject trail = scene->CreateGameObject("Trail", &prop);
    trail.GetComponent<CTransform>().SetPosition(glm::vec3(0.0f, 0.0f, 2.0f));

    scene->Update(0.5f);
    scene->UpdateWorldMatrices();

    const glm::vec3 t = TranslationOf(trail.GetComponent<CTransform>().worldMatrix);
    EXPECT_FLOAT_EQ(t.x, 5.0f);
    EXPECT_FLOAT_EQ(t.y, 1.0f);
    EXPECT_FLOAT_EQ(t.z, 2.0f);
}

// THE dirty-flag test, and the reason it exists: an animating character's transform
// never changes -- the POSE moves, not the object -- so nothing sets m_localDirty on
// the prop after frame one. A cached implementation freezes it at the bone's first
// position and passes every other test in this file.
TEST_F(t_CBoneAttachment, AttachedPropTracksTheBoneAcrossFramesWithNoTransformChange)
{
    ResourceSkeleton  rig(1);   MakeTwoBoneRig(rig);
    ResourceAnimation anim(2);  MakeSlideClip(anim, "Child");

    GameObject character = scene->CreateGameObject("Character");
    auto& animator = character.AddComponent<CAnimator>();
    animator.skeleton = &rig;
    animator.clip     = &anim;

    GameObject prop = scene->CreateGameObject("Sword", &character);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->Update(0.25f);
    scene->UpdateWorldMatrices();
    ASSERT_FLOAT_EQ(TranslationOf(prop.GetComponent<CTransform>().worldMatrix).x, 2.5f);

    // Second frame: nothing touches any transform, only the pose advances.
    scene->Update(0.25f);
    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(prop.GetComponent<CTransform>().worldMatrix).x, 5.0f);
}

// Resolution is deliberately uncached, and this is the property that justifies it:
// reparenting must re-resolve with no explicit call from the editor.
TEST_F(t_CBoneAttachment, ReparentingUnderAnotherAnimatorReResolves)
{
    ResourceSkeleton  rig(1);       MakeTwoBoneRig(rig);
    ResourceAnimation slowAnim(2);  MakeSlideClip(slowAnim, "Child", 10.0f);
    ResourceAnimation fastAnim(3);  MakeSlideClip(fastAnim, "Child", 40.0f);

    GameObject slow = scene->CreateGameObject("Slow");
    auto& slowAnimator = slow.AddComponent<CAnimator>();
    slowAnimator.skeleton = &rig;
    slowAnimator.clip     = &slowAnim;

    GameObject fast = scene->CreateGameObject("Fast");
    auto& fastAnimator = fast.AddComponent<CAnimator>();
    fastAnimator.skeleton = &rig;
    fastAnimator.clip     = &fastAnim;

    GameObject prop = scene->CreateGameObject("Sword", &slow);
    prop.AddComponent<CBoneAttachment>().boneName = "Child";

    scene->Update(0.5f);
    scene->UpdateWorldMatrices();
    ASSERT_FLOAT_EQ(TranslationOf(prop.GetComponent<CTransform>().worldMatrix).x, 5.0f);

    prop.SetParent(fast);

    scene->UpdateWorldMatrices();

    EXPECT_FLOAT_EQ(TranslationOf(prop.GetComponent<CTransform>().worldMatrix).x, 20.0f);
}

// An unresolved attachment must leave ordinary parenting working, or a typo'd bone
// name would teleport the prop to the origin instead of merely failing to attach.
TEST_F(t_CBoneAttachment, UnresolvedAttachmentStillInheritsTheParentTransform)
{
    GameObject parent = scene->CreateGameObject("Empty");
    parent.GetComponent<CTransform>().SetPosition(glm::vec3(7.0f, 0.0f, 0.0f));

    GameObject prop = scene->CreateGameObject("Sword", &parent);
    prop.AddComponent<CBoneAttachment>().boneName = "NoSuchBone";
    prop.GetComponent<CTransform>().SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));

    scene->UpdateWorldMatrices();

    const glm::vec3 t = TranslationOf(prop.GetComponent<CTransform>().worldMatrix);
    EXPECT_FLOAT_EQ(t.x, 7.0f);
    EXPECT_FLOAT_EQ(t.y, 1.0f);
}

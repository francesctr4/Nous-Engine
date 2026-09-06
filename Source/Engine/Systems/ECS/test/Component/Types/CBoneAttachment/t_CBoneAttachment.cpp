#include <gtest/gtest.h>

#include <ECS/Component/Types/CBoneAttachment/CBoneAttachment.h>
#include <ECS/GameObject.h>
#include <ECS/Scene/Scene.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>

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

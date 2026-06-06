#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CAudioListener/include/CAudioListener.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// CAudioListener reaches the audio backend only through ModuleScene::GetAudio().
// A standalone Scene has no ModuleScene wired, so OnUpdate safely no-ops here —
// these tests cover defaults, ECS mechanics, and serialization.

class t_CAudioListener : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }
    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        nous::engine::memory::ShutdownMemory();
    }
    Scene* scene = nullptr;
};

TEST_F(t_CAudioListener, DefaultIsMainListener_IsTrue)
{
    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CAudioListener>();
    EXPECT_TRUE(go.GetComponent<CAudioListener>().isMainListener);
}

TEST_F(t_CAudioListener, GetType_IsCAudioListener)
{
    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CAudioListener>();
    EXPECT_EQ(go.GetComponent<CAudioListener>().GetType(), "CAudioListener");
}

TEST_F(t_CAudioListener, Serialize_WritesTypeName)
{
    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CAudioListener>();
    const JsonObject json = go.GetComponent<CAudioListener>().Serialize();
    EXPECT_EQ(json.GetString("type"), "CAudioListener");
}

TEST_F(t_CAudioListener, SerializeRoundTrip_PreservesIsMainListener)
{
    GameObject src = scene->CreateGameObject("Src");
    auto& a = src.AddComponent<CAudioListener>();
    a.isMainListener = false;

    const JsonObject json = a.Serialize();

    GameObject dst = scene->CreateGameObject("Dst");
    auto& b = dst.AddComponent<CAudioListener>();
    b.Deserialize(json);

    EXPECT_FALSE(b.isMainListener);
}

TEST_F(t_CAudioListener, Deserialize_MissingKey_KeepsDefault)
{
    GameObject go = scene->CreateGameObject("Listener");
    auto& a = go.AddComponent<CAudioListener>();
    JsonObject empty;
    a.Deserialize(empty);
    EXPECT_TRUE(a.isMainListener);  // default preserved
}

TEST_F(t_CAudioListener, AddRemove_HasComponentTracksCorrectly)
{
    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CAudioListener>();
    EXPECT_TRUE(go.HasComponent<CAudioListener>());
    go.RemoveComponent<CAudioListener>();
    EXPECT_FALSE(go.HasComponent<CAudioListener>());
}

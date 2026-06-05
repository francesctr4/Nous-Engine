#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/include/CVideoPlayer.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// CVideoPlayer talks to the video backend only through ModuleScene::GetVideo().
// A standalone Scene has no ModuleScene wired, so OnUpdate/OnDestroy safely no-op
// here — these tests cover defaults, ECS mechanics, and field serialization.

class t_CVideoPlayer : public ::testing::Test
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

TEST_F(t_CVideoPlayer, DefaultLoop_IsFalse)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    EXPECT_FALSE(go.GetComponent<CVideoPlayer>().loop);
}

TEST_F(t_CVideoPlayer, DefaultPlayOnAwake_IsTrue)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    EXPECT_TRUE(go.GetComponent<CVideoPlayer>().playOnAwake);
}

TEST_F(t_CVideoPlayer, DefaultTargetSlot_IsDiffuseSampler)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    EXPECT_EQ(go.GetComponent<CVideoPlayer>().targetSlot, "diffuseSampler");
}

TEST_F(t_CVideoPlayer, DefaultClipAndHandle_AreNull)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    EXPECT_EQ(go.GetComponent<CVideoPlayer>().clip, nullptr);
    EXPECT_EQ(go.GetComponent<CVideoPlayer>().handle, nullptr);
}

TEST_F(t_CVideoPlayer, GetType_IsCVideoPlayer)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    EXPECT_EQ(go.GetComponent<CVideoPlayer>().GetType(), "CVideoPlayer");
}

TEST_F(t_CVideoPlayer, Serialize_WritesTypeName)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    const JsonObject json = go.GetComponent<CVideoPlayer>().Serialize();
    EXPECT_EQ(json.GetString("type"), "CVideoPlayer");
}

TEST_F(t_CVideoPlayer, SerializeRoundTrip_PreservesFields)
{
    GameObject src = scene->CreateGameObject("Src");
    auto& a = src.AddComponent<CVideoPlayer>();
    a.loop        = true;
    a.playOnAwake = false;
    a.targetSlot  = "albedoSampler";

    const JsonObject json = a.Serialize();

    GameObject dst = scene->CreateGameObject("Dst");
    auto& b = dst.AddComponent<CVideoPlayer>();
    b.Deserialize(json);

    EXPECT_TRUE(b.loop);
    EXPECT_FALSE(b.playOnAwake);
    EXPECT_EQ(b.targetSlot, "albedoSampler");
}

TEST_F(t_CVideoPlayer, AddRemove_HasComponentTracks)
{
    GameObject go = scene->CreateGameObject("Video");
    go.AddComponent<CVideoPlayer>();
    EXPECT_TRUE(go.HasComponent<CVideoPlayer>());
    go.RemoveComponent<CVideoPlayer>();
    EXPECT_FALSE(go.HasComponent<CVideoPlayer>());
}

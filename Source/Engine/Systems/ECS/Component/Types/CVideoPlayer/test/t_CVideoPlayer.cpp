#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/include/CVideoPlayer.h"
#include "Engine/Systems/ECS/test/FakeComponentServices.h"
#include "Engine/Systems/ResourceManager/Types/ResourceVideo/include/ResourceVideo.h"
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// CVideoPlayer reaches the decoder through the ComponentServices seam, so these
// tests wire fake brokers into the Scene and assert on the decoder-handle
// lifecycle the component actually drives.

class t_CVideoPlayer : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene", &fakes.services);
        clip  = NOUS_NEW<ResourceVideo>(MemoryTag::RESOURCE_VIDEO, 11u);
        clip->SetDurationSec(10.0f);
    }
    void TearDown() override
    {
        // Scene first: component OnDestroy runs while the clip is still alive.
        NOUS_DELETE(scene, MemoryTag::SCENE);
        NOUS_DELETE(clip, MemoryTag::RESOURCE_VIDEO);
        nous::engine::memory::ShutdownMemory();
    }

    // Declared before `scene` so it outlives it — the Scene holds a pointer into it.
    FakeServices   fakes;
    Scene*         scene = nullptr;
    ResourceVideo* clip  = nullptr;
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

// =============================================================================
// Decoder-handle lifecycle (requires the ComponentServices seam)
// =============================================================================

TEST_F(t_CVideoPlayer, OnUpdate_PlayingWithClipAndPlayOnAwake_CreatesAndStartsDecoder)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = clip;
    v.playOnAwake = true;

    v.OnUpdate(0.016f);

    EXPECT_EQ(fakes.video.CountOf("CreateVideo"), 1);
    EXPECT_EQ(fakes.video.CountOf("Start"),       1);
    EXPECT_TRUE(fakes.video.Called("SetLooping"));
    EXPECT_NE(v.handle, nullptr);
}

TEST_F(t_CVideoPlayer, OnUpdate_CalledRepeatedly_CreatesDecoderOnlyOnce)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = clip;
    v.playOnAwake = true;

    v.OnUpdate(0.016f);
    v.OnUpdate(0.016f);
    v.OnUpdate(0.016f);

    EXPECT_EQ(fakes.video.CountOf("CreateVideo"), 1);
    EXPECT_EQ(fakes.video.CountOf("Start"),       1);
}

TEST_F(t_CVideoPlayer, OnUpdate_PlayingThenStopped_DestroysDecoderAndResets)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = clip;
    v.playOnAwake = true;

    v.OnUpdate(0.016f);
    ASSERT_NE(v.handle, nullptr);

    fakes.host.SetStopped();
    v.OnUpdate(0.016f);

    EXPECT_EQ(fakes.video.CountOf("DestroyVideo"), 1);
    EXPECT_EQ(v.handle, nullptr);
    EXPECT_DOUBLE_EQ(v.playhead, 0.0);
    EXPECT_FALSE(v.frameDirty);
}

TEST_F(t_CVideoPlayer, OnUpdate_WhilePlaying_PullsFrames)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = clip;
    v.playOnAwake = true;

    v.OnUpdate(0.016f);

    EXPECT_TRUE(fakes.video.Called("TryGetFrame"));
    // The fake reports no frame available — the component must cope, not latch.
    EXPECT_FALSE(v.frameDirty);
}

TEST_F(t_CVideoPlayer, OnUpdate_NoClip_DoesNotCreateDecoder)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = nullptr;
    v.playOnAwake = true;

    v.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.video.Called("CreateVideo"));
}

TEST_F(t_CVideoPlayer, OnUpdate_PlayOnAwakeFalse_DoesNotCreateDecoder)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = clip;
    v.playOnAwake = false;

    v.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.video.Called("CreateVideo"));
}

TEST_F(t_CVideoPlayer, OnUpdate_WhilePaused_HoldsPlayheadAndPullsNothing)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip        = clip;
    v.playOnAwake = true;
    v.OnUpdate(0.5f);

    const double heldPlayhead = v.playhead;

    fakes.host.SetPaused();
    const int framePullsBefore = fakes.video.CountOf("TryGetFrame");
    v.OnUpdate(0.5f);

    EXPECT_EQ(fakes.video.CountOf("TryGetFrame"), framePullsBefore);
    EXPECT_DOUBLE_EQ(v.playhead, heldPlayhead);
    // PAUSED holds the decoder — only STOPPED tears it down.
    EXPECT_EQ(fakes.video.CountOf("DestroyVideo"), 0);
}

TEST_F(t_CVideoPlayer, OnUpdate_WhileStoppedWithNoHandle_DestroysNothing)
{
    fakes.host.SetStopped();

    GameObject go = scene->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();

    v.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.video.Called("DestroyVideo"));
    EXPECT_FALSE(fakes.video.Called("CreateVideo"));
}

TEST_F(t_CVideoPlayer, OnUpdate_UnwiredScene_DoesNotCrash)
{
    // The all-null aggregate must stay a supported state: every ECS fixture
    // outside this file constructs a Scene with nothing wired.
    Scene* bare = NOUS_NEW<Scene>(MemoryTag::SCENE, "Bare");
    GameObject go = bare->CreateGameObject("Video");
    auto& v = go.AddComponent<CVideoPlayer>();
    v.clip = clip;

    v.OnUpdate(0.016f);

    EXPECT_EQ(v.handle, nullptr);
    NOUS_DELETE(bare, MemoryTag::SCENE);
}

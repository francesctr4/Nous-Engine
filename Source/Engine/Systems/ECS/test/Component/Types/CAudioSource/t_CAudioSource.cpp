#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAudioSource/CAudioSource.h>
#include <AudioSystem/AudioTypes.h>
#include <FakeComponentServices.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>
#include <MemoryManager/MemoryManager.h>
#include <Utils/Serialization/JsonObject.h>

// CAudioSource reaches the audio engine through the ComponentServices seam, so
// these tests wire fake brokers into the Scene and assert on the voice-lifecycle
// calls the component actually makes.

class t_CAudioSource : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene", &fakes.services);
        clip  = NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO, 7u);
        clip->SetDurationSec(4.0f);
    }

    void TearDown() override
    {
        // Scene first: component OnDestroy runs while the clip is still alive.
        NOUS_DELETE(scene, MemoryTag::SCENE);
        NOUS_DELETE(clip, MemoryTag::RESOURCE_AUDIO);
        nous::engine::memory::ShutdownMemory();
    }

    // Declared before `scene` so it outlives it — the Scene holds a pointer into it.
    FakeServices   fakes;
    Scene*         scene = nullptr;
    ResourceAudio* clip  = nullptr;
};

// =============================================================================
// Default state
// =============================================================================

TEST_F(t_CAudioSource, DefaultVolume_Is1)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_FLOAT_EQ(go.GetComponent<CAudioSource>().volume, 1.0f);
}

TEST_F(t_CAudioSource, DefaultPitch_Is1)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_FLOAT_EQ(go.GetComponent<CAudioSource>().pitch, 1.0f);
}

TEST_F(t_CAudioSource, DefaultLoop_IsFalse)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_FALSE(go.GetComponent<CAudioSource>().loop);
}

TEST_F(t_CAudioSource, DefaultPlayOnAwake_IsTrue)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_TRUE(go.GetComponent<CAudioSource>().playOnAwake);
}

TEST_F(t_CAudioSource, DefaultClip_IsNull)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_EQ(go.GetComponent<CAudioSource>().clip, nullptr);
}

TEST_F(t_CAudioSource, GetType_IsCAudioSource)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_EQ(go.GetComponent<CAudioSource>().GetType(), "CAudioSource");
}

// =============================================================================
// Serialization (clip-less: covers the POD field round-trip)
// =============================================================================

TEST_F(t_CAudioSource, Serialize_WritesTypeName)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    const JsonObject json = go.GetComponent<CAudioSource>().Serialize();
    EXPECT_EQ(json.GetString("type"), "CAudioSource");
}

TEST_F(t_CAudioSource, SerializeRoundTrip_PreservesFields)
{
    GameObject src = scene->CreateGameObject("Src");
    auto& a = src.AddComponent<CAudioSource>();
    a.volume      = 0.25f;
    a.pitch       = 1.75f;
    a.loop        = true;
    a.playOnAwake = false;

    const JsonObject json = a.Serialize();

    GameObject dst = scene->CreateGameObject("Dst");
    auto& b = dst.AddComponent<CAudioSource>();
    b.Deserialize(json);

    EXPECT_FLOAT_EQ(b.volume, 0.25f);
    EXPECT_FLOAT_EQ(b.pitch,  1.75f);
    EXPECT_TRUE(b.loop);
    EXPECT_FALSE(b.playOnAwake);
}

TEST_F(t_CAudioSource, Deserialize_EmptyClipFields_LeavesClipNull)
{
    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();

    JsonObject json;
    json.Set("volume", 0.5f);  // no assetPath / libraryPath
    a.Deserialize(json);

    EXPECT_EQ(a.clip, nullptr);
    EXPECT_FLOAT_EQ(a.volume, 0.5f);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CAudioSource, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_TRUE(go.HasComponent<CAudioSource>());
}

TEST_F(t_CAudioSource, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoAudio");
    EXPECT_EQ(go.TryGetComponent<CAudioSource>(), nullptr);
}

TEST_F(t_CAudioSource, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    go.RemoveComponent<CAudioSource>();
    EXPECT_FALSE(go.HasComponent<CAudioSource>());
}

TEST_F(t_CAudioSource, MultipleGameObjects_IndependentState)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CAudioSource>();
    b.AddComponent<CAudioSource>();
    b.GetComponent<CAudioSource>().volume = 0.1f;
    EXPECT_FLOAT_EQ(a.GetComponent<CAudioSource>().volume, 1.0f);
    EXPECT_FLOAT_EQ(b.GetComponent<CAudioSource>().volume, 0.1f);
}

// =============================================================================
// Spatialization fields (Step 4)
// =============================================================================

TEST_F(t_CAudioSource, DefaultSpatialize_IsFalse)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_FALSE(go.GetComponent<CAudioSource>().spatialize);
}

TEST_F(t_CAudioSource, DefaultDistances_AreOneAndFifty)
{
    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    EXPECT_FLOAT_EQ(a.minDistance, 1.0f);
    EXPECT_FLOAT_EQ(a.maxDistance, 50.0f);
}

TEST_F(t_CAudioSource, DefaultAttenuation_IsInverse)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_EQ(go.GetComponent<CAudioSource>().attenuation, AttenuationModel::Inverse);
}

TEST_F(t_CAudioSource, SerializeRoundTrip_PreservesSpatialFields)
{
    GameObject src = scene->CreateGameObject("Src");
    auto& a = src.AddComponent<CAudioSource>();
    a.spatialize  = true;
    a.minDistance = 2.5f;
    a.maxDistance = 80.0f;
    a.attenuation = AttenuationModel::Exponential;

    const JsonObject json = a.Serialize();

    GameObject dst = scene->CreateGameObject("Dst");
    auto& b = dst.AddComponent<CAudioSource>();
    b.Deserialize(json);

    EXPECT_TRUE(b.spatialize);
    EXPECT_FLOAT_EQ(b.minDistance, 2.5f);
    EXPECT_FLOAT_EQ(b.maxDistance, 80.0f);
    EXPECT_EQ(b.attenuation, AttenuationModel::Exponential);
}

TEST_F(t_CAudioSource, Deserialize_MissingSpatialKeys_KeepsDefaults)
{
    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    JsonObject json;
    json.Set("volume", 0.5f);  // no spatial keys
    a.Deserialize(json);
    EXPECT_FALSE(a.spatialize);
    EXPECT_FLOAT_EQ(a.minDistance, 1.0f);
    EXPECT_FLOAT_EQ(a.maxDistance, 50.0f);
    EXPECT_EQ(a.attenuation, AttenuationModel::Inverse);
}

// =============================================================================
// Bus routing (Step 6)
// =============================================================================

TEST_F(t_CAudioSource, DefaultTargetBus_IsSFX)
{
    GameObject go = scene->CreateGameObject("Audio");
    go.AddComponent<CAudioSource>();
    EXPECT_EQ(go.GetComponent<CAudioSource>().targetBus, AudioBus::SFX);
}

TEST_F(t_CAudioSource, SerializeRoundTrip_PreservesTargetBus)
{
    GameObject src = scene->CreateGameObject("Src");
    auto& a = src.AddComponent<CAudioSource>();
    a.targetBus = AudioBus::Music;

    const JsonObject json = a.Serialize();

    GameObject dst = scene->CreateGameObject("Dst");
    auto& b = dst.AddComponent<CAudioSource>();
    b.Deserialize(json);

    EXPECT_EQ(b.targetBus, AudioBus::Music);
}

TEST_F(t_CAudioSource, Deserialize_MissingTargetBus_KeepsSFXDefault)
{
    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    JsonObject json;
    json.Set("volume", 0.5f);  // no targetBus key
    a.Deserialize(json);
    EXPECT_EQ(a.targetBus, AudioBus::SFX);
}

// =============================================================================
// Voice lifecycle (requires the ComponentServices seam)
// =============================================================================

TEST_F(t_CAudioSource, OnUpdate_PlayingWithClipAndPlayOnAwake_CreatesAndStartsVoice)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;

    a.OnUpdate(0.016f);

    EXPECT_EQ(fakes.audio.CountOf("CreateSound"), 1);
    EXPECT_EQ(fakes.audio.CountOf("StartSound"),  1);
    EXPECT_TRUE(fakes.audio.Called("SetSoundLooping"));
}

TEST_F(t_CAudioSource, OnUpdate_NoClip_DoesNotCreateVoice)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = nullptr;
    a.playOnAwake = true;

    a.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.audio.Called("CreateSound"));
}

TEST_F(t_CAudioSource, OnUpdate_PlayOnAwakeFalse_DoesNotCreateVoice)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = false;

    a.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.audio.Called("CreateSound"));
}

TEST_F(t_CAudioSource, OnUpdate_CalledRepeatedly_CreatesVoiceOnlyOnce)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;

    a.OnUpdate(0.016f);
    a.OnUpdate(0.016f);
    a.OnUpdate(0.016f);

    EXPECT_EQ(fakes.audio.CountOf("CreateSound"), 1);
    EXPECT_EQ(fakes.audio.CountOf("StartSound"),  1);
}

TEST_F(t_CAudioSource, OnUpdate_PlayingThenStopped_ReleasesTheVoice)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;

    a.OnUpdate(0.016f);
    ASSERT_EQ(fakes.audio.CountOf("CreateSound"), 1);

    fakes.host.SetStopped();
    a.OnUpdate(0.016f);

    EXPECT_EQ(fakes.audio.CountOf("DestroySound"), 1);
}

TEST_F(t_CAudioSource, OnUpdate_PlayingThenPaused_StopsVoiceWithoutReleasingIt)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;

    a.OnUpdate(0.016f);

    fakes.host.SetPaused();
    a.OnUpdate(0.016f);

    // PAUSED retains the cursor: the voice is stopped, never destroyed.
    EXPECT_EQ(fakes.audio.CountOf("StopSound"),    1);
    EXPECT_EQ(fakes.audio.CountOf("DestroySound"), 0);
}

TEST_F(t_CAudioSource, OnUpdate_PausedThenPlaying_ResumesWithoutRecreating)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;

    a.OnUpdate(0.016f);
    fakes.host.SetPaused();
    a.OnUpdate(0.016f);
    fakes.host.SetPlaying();
    a.OnUpdate(0.016f);

    EXPECT_EQ(fakes.audio.CountOf("CreateSound"), 1);
    EXPECT_EQ(fakes.audio.CountOf("StartSound"),  2);  // initial start + resume
}

TEST_F(t_CAudioSource, OnUpdate_WhilePlaying_PushesVolumeAndPitchEveryFrame)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;
    a.volume      = 0.25f;
    a.pitch       = 1.5f;

    a.OnUpdate(0.016f);

    EXPECT_FLOAT_EQ(fakes.audio.lastVolume, 0.25f);
    EXPECT_FLOAT_EQ(fakes.audio.lastPitch,  1.5f);
}

TEST_F(t_CAudioSource, OnUpdate_Spatialized_PushesTransformPosition)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& t = go.GetComponent<CTransform>();
    t.position = glm::vec3(4.0f, 5.0f, 6.0f);

    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;
    a.spatialize  = true;

    a.OnUpdate(0.016f);

    EXPECT_TRUE(fakes.audio.Called("SetSoundPosition"));
    EXPECT_FLOAT_EQ(fakes.audio.lastPosition[0], 4.0f);
    EXPECT_FLOAT_EQ(fakes.audio.lastPosition[1], 5.0f);
    EXPECT_FLOAT_EQ(fakes.audio.lastPosition[2], 6.0f);
}

TEST_F(t_CAudioSource, OnUpdate_NotSpatialized_DoesNotPushPosition)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;
    a.playOnAwake = true;
    a.spatialize  = false;

    a.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.audio.Called("SetSoundPosition"));
}

// =============================================================================
// Sibling-sync surface (read by CVideoPlayer)
// =============================================================================

TEST_F(t_CAudioSource, IsVoicePlaying_NoVoice_IsFalse)
{
    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    EXPECT_FALSE(a.IsVoicePlaying());
}

TEST_F(t_CAudioSource, GetPlaybackSeconds_FoldsMonotonicCursorIntoClipDuration)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Audio");
    auto& a = go.AddComponent<CAudioSource>();
    a.clip        = clip;          // duration 4.0s
    a.playOnAwake = true;
    a.OnUpdate(0.016f);

    // miniaudio's cursor grows monotonically across loops — it does NOT wrap.
    // GetPlaybackSeconds must fold it, or a synced video races at decode speed.
    fakes.audio.cursor = 9.0;

    EXPECT_DOUBLE_EQ(a.GetPlaybackSeconds(), 1.0);  // fmod(9.0, 4.0)
}

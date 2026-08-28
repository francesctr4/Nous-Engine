#include <gtest/gtest.h>

#include <FakeComponentServices.h>
#include <ECS/Scene/Scene.h>
#include <MemoryManager/MemoryManager.h>
#include <EngineCore/Globals.h>

class t_FakeComponentServices : public ::testing::Test
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

    // Declared before `scene` so it is destroyed after it — the Scene holds a
    // pointer into this object for its whole lifetime.
    FakeServices fakes;
    Scene*       scene = nullptr;
};

// =============================================================================
// Delivery
// =============================================================================

TEST_F(t_FakeComponentServices, WiredScene_ExposesEveryFake)
{
    const ComponentServices& s = scene->GetServices();
    EXPECT_EQ(s.host,      &fakes.host);
    EXPECT_EQ(s.audio,     &fakes.audio);
    EXPECT_EQ(s.video,     &fakes.video);
    EXPECT_EQ(s.resources, &fakes.resources);
    EXPECT_EQ(s.scripts,   &fakes.scripts);
}

// =============================================================================
// FakeSceneHost
// =============================================================================

TEST_F(t_FakeComponentServices, FakeSceneHost_StoppedByDefault)
{
    EXPECT_TRUE(fakes.host.IsStopped());
    EXPECT_FALSE(fakes.host.IsPlaying());
    EXPECT_FALSE(fakes.host.IsPaused());
}

TEST_F(t_FakeComponentServices, FakeSceneHost_SetPlaying_FlipsPredicates)
{
    fakes.host.SetPlaying();
    EXPECT_TRUE(fakes.host.IsPlaying());
    EXPECT_FALSE(fakes.host.IsStopped());
}

TEST_F(t_FakeComponentServices, FakeSceneHost_SetPaused_IsNeitherPlayingNorStopped)
{
    fakes.host.SetPaused();
    EXPECT_TRUE(fakes.host.IsPaused());
    EXPECT_FALSE(fakes.host.IsPlaying());
    EXPECT_FALSE(fakes.host.IsStopped());
}

// =============================================================================
// FakeAudioBroker
// =============================================================================

TEST_F(t_FakeComponentServices, FakeAudioBroker_RecordsCallsAndHandsOutHandles)
{
    const SoundHandle a = fakes.audio.CreateSound(nullptr, AudioBus::SFX);
    const SoundHandle b = fakes.audio.CreateSound(nullptr, AudioBus::SFX);
    EXPECT_NE(a, b);
    EXPECT_NE(a, nullptr);
    EXPECT_EQ(fakes.audio.CountOf("CreateSound"), 2);
    EXPECT_FALSE(fakes.audio.Called("StartSound"));
}

TEST_F(t_FakeComponentServices, FakeAudioBroker_StartStop_TracksPlayingState)
{
    const SoundHandle h = fakes.audio.CreateSound(nullptr, AudioBus::SFX);
    EXPECT_FALSE(fakes.audio.IsSoundPlaying(h));

    fakes.audio.StartSound(h);
    EXPECT_TRUE(fakes.audio.IsSoundPlaying(h));

    fakes.audio.StopSound(h);
    EXPECT_FALSE(fakes.audio.IsSoundPlaying(h));
}

TEST_F(t_FakeComponentServices, FakeAudioBroker_RecordsLastVolumeAndPitch)
{
    const SoundHandle h = fakes.audio.CreateSound(nullptr, AudioBus::SFX);
    fakes.audio.SetSoundVolume(h, 0.25f);
    fakes.audio.SetSoundPitch(h, 1.5f);

    EXPECT_FLOAT_EQ(fakes.audio.lastVolume, 0.25f);
    EXPECT_FLOAT_EQ(fakes.audio.lastPitch,  1.5f);
}

// =============================================================================
// FakeVideoBroker / FakeResourceLoader
// =============================================================================

TEST_F(t_FakeComponentServices, FakeVideoBroker_TryGetFrame_ReportsNoFrame)
{
    VideoFrame  frame{};
    const bool  got = fakes.video.TryGetFrame(nullptr, 0.0, frame);

    EXPECT_FALSE(got);
    EXPECT_TRUE(fakes.video.Called("TryGetFrame"));
}

TEST_F(t_FakeComponentServices, FakeResourceLoader_UnloadResource_RecordsTheUID)
{
    EXPECT_TRUE(fakes.resources.UnloadResource(42u));

    ASSERT_EQ(fakes.resources.unloaded.size(), 1u);
    EXPECT_EQ(fakes.resources.unloaded[0], 42u);
}

TEST_F(t_FakeComponentServices, FakeScriptRegistry_RegisterUnregister_BalancesToZero)
{
    fakes.scripts.RegisterScriptComponent(nullptr);
    EXPECT_EQ(fakes.scripts.registered, 1);

    fakes.scripts.UnregisterScriptComponent(nullptr);
    EXPECT_EQ(fakes.scripts.registered, 0);
}

#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAudioListener/CAudioListener.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>
#include <EngineCore/Globals.h>
#include <Utils/Serialization/JsonObject.h>

// CAudioListener reaches the audio engine through the ComponentServices seam, so
// these tests wire fake brokers into the Scene and assert on the calls the
// component actually makes — not merely that it survives a headless scene.

class t_CAudioListener : public ::testing::Test
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

    // Declared before `scene` so it outlives it — the Scene holds a pointer into it.
    FakeServices fakes;
    Scene*       scene = nullptr;
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

// =============================================================================
// Listener push behavior (requires the ComponentServices seam)
// =============================================================================

TEST_F(t_CAudioListener, OnUpdate_WhilePlayingAndMain_PushesListenerOnce)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CTransform>();
    auto& l = go.AddComponent<CAudioListener>();
    l.isMainListener = true;

    l.OnUpdate(0.016f);

    EXPECT_EQ(fakes.audio.CountOf("SetListenerPosition"),  1);
    EXPECT_EQ(fakes.audio.CountOf("SetListenerDirection"), 1);
    EXPECT_EQ(fakes.audio.CountOf("SetListenerWorldUp"),   1);
}

TEST_F(t_CAudioListener, OnUpdate_WhileStopped_PushesNothing)
{
    fakes.host.SetStopped();

    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CTransform>();
    auto& l = go.AddComponent<CAudioListener>();
    l.isMainListener = true;

    l.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.audio.Called("SetListenerPosition"));
}

TEST_F(t_CAudioListener, OnUpdate_NotMainListener_PushesNothing)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CTransform>();
    auto& l = go.AddComponent<CAudioListener>();
    l.isMainListener = false;

    l.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.audio.Called("SetListenerPosition"));
}

TEST_F(t_CAudioListener, OnUpdate_PushesTransformPosition)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Listener");
    auto& t = go.GetComponent<CTransform>();
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    auto& l = go.AddComponent<CAudioListener>();
    l.isMainListener = true;

    l.OnUpdate(0.016f);

    EXPECT_FLOAT_EQ(fakes.audio.lastListenerPosition[0], 1.0f);
    EXPECT_FLOAT_EQ(fakes.audio.lastListenerPosition[1], 2.0f);
    EXPECT_FLOAT_EQ(fakes.audio.lastListenerPosition[2], 3.0f);
}

TEST_F(t_CAudioListener, OnUpdate_NoTransform_PushesNothing)
{
    fakes.host.SetPlaying();

    GameObject go = scene->CreateGameObject("Listener");
    auto& l = go.AddComponent<CAudioListener>();
    l.isMainListener = true;

    // CreateGameObject always attaches a CTransform, so the component's null-transform
    // guard is only reachable by removing it.
    go.RemoveComponent<CTransform>();

    l.OnUpdate(0.016f);

    EXPECT_FALSE(fakes.audio.Called("SetListenerPosition"));
}

TEST_F(t_CAudioListener, OnUpdate_WhilePaused_StillPushes)
{
    // PAUSED is not STOPPED: voices still exist and keep their cursor, so the
    // listener must stay current or a resumed scene pans from a stale position.
    fakes.host.SetPaused();

    GameObject go = scene->CreateGameObject("Listener");
    go.AddComponent<CTransform>();
    auto& l = go.AddComponent<CAudioListener>();
    l.isMainListener = true;

    l.OnUpdate(0.016f);

    EXPECT_EQ(fakes.audio.CountOf("SetListenerPosition"), 1);
}

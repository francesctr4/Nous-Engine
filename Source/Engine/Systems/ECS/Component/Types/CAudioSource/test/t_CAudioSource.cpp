#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/CAudioSource.h"
#include "Engine/Systems/AudioSystem/AudioTypes.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// CAudioSource talks to the audio backend only through ModuleScene::GetAudio().
// A standalone Scene has no ModuleScene wired, so OnUpdate/OnDestroy safely no-op
// here — these tests cover defaults, ECS mechanics, and field serialization,
// which is everything testable without a live audio device.

class t_CAudioSource : public ::testing::Test
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

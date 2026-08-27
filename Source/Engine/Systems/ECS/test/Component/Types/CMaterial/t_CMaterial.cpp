#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <FakeComponentServices.h>
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"
#include <Utils/Serialization/JsonObject.h>

class t_CMaterial : public ::testing::Test
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

// =============================================================================
// Default state
// =============================================================================

TEST_F(t_CMaterial, DefaultMaterial_IsNull)
{
    GameObject go = scene->CreateGameObject("Mat");
    go.AddComponent<CMaterial>();
    EXPECT_EQ(go.GetComponent<CMaterial>().material, nullptr);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CMaterial, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Mat");
    go.AddComponent<CMaterial>();
    EXPECT_TRUE(go.HasComponent<CMaterial>());
}

TEST_F(t_CMaterial, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Mat");
    go.AddComponent<CMaterial>();
    CMaterial& ref1 = go.GetComponent<CMaterial>();
    CMaterial& ref2 = go.GetComponent<CMaterial>();
    EXPECT_EQ(&ref1, &ref2);
}

TEST_F(t_CMaterial, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoCMat");
    EXPECT_EQ(go.TryGetComponent<CMaterial>(), nullptr);
}

TEST_F(t_CMaterial, TryGetComponent_NonNullWhenPresent)
{
    GameObject go = scene->CreateGameObject("WithCMat");
    go.AddComponent<CMaterial>();
    EXPECT_NE(go.TryGetComponent<CMaterial>(), nullptr);
}

TEST_F(t_CMaterial, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("RemoveMat");
    go.AddComponent<CMaterial>();
    ASSERT_TRUE(go.HasComponent<CMaterial>());
    go.RemoveComponent<CMaterial>();
    EXPECT_FALSE(go.HasComponent<CMaterial>());
}

TEST_F(t_CMaterial, MultipleGameObjects_IndependentMaterial)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CMaterial>();
    b.AddComponent<CMaterial>();
    EXPECT_NE(&a.GetComponent<CMaterial>(), &b.GetComponent<CMaterial>());
}

// =============================================================================
// Resource resolution (requires the ComponentServices seam)
// =============================================================================

TEST_F(t_CMaterial, Deserialize_EmptyAssetPath_RestoresTheDefaultMaterial)
{
    // A GO that used the in-memory default material serializes an empty assetPath;
    // it must come back as the default, not as nullptr, or the mesh stops rendering
    // after a snapshot round-trip.
    GameObject go = scene->CreateGameObject("Mat");
    auto& m = go.AddComponent<CMaterial>();

    JsonObject json;
    json.Set("assetPath", "");
    m.Deserialize(json);

    EXPECT_TRUE(fakes.resources.Called("GetDefaultMaterial"));
}

TEST_F(t_CMaterial, Deserialize_WithAssetPath_ResolvesThroughTheLoader)
{
    GameObject go = scene->CreateGameObject("Mat");
    auto& m = go.AddComponent<CMaterial>();

    JsonObject json;
    json.Set("assetPath", "Assets/Materials/stone.nmat");
    m.Deserialize(json);

    EXPECT_TRUE(fakes.resources.Called("CreateResource"));
}

TEST_F(t_CMaterial, Deserialize_WithLibraryPath_PrefersTheLibraryCall)
{
    GameObject go = scene->CreateGameObject("Mat");
    auto& m = go.AddComponent<CMaterial>();

    JsonObject json;
    json.Set("assetPath",   "Assets/Materials/stone.nmat");
    json.Set("libraryPath", "Library/Materials/5.nmat");
    json.Set("resourceUID", 5.0);
    m.Deserialize(json);

    EXPECT_TRUE(fakes.resources.Called("CreateResourceFromLibrary"));
}

TEST_F(t_CMaterial, Deserialize_UnwiredScene_DoesNotCrash)
{
    // No resource loader wired: the component must no-op, not dereference null.
    Scene* bare = NOUS_NEW<Scene>(MemoryTag::SCENE, "Bare");
    GameObject go = bare->CreateGameObject("Mat");
    auto& m = go.AddComponent<CMaterial>();

    JsonObject json;
    json.Set("assetPath", "Assets/Materials/stone.nmat");
    m.Deserialize(json);

    EXPECT_EQ(m.material, nullptr);
    NOUS_DELETE(bare, MemoryTag::SCENE);
}

#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/test/FakeComponentServices.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

class t_CMesh : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene", nullptr, nullptr, &fakes.services);
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

TEST_F(t_CMesh, DefaultMesh_IsNull)
{
    GameObject go = scene->CreateGameObject("Mesh");
    go.AddComponent<CMesh>();
    EXPECT_EQ(go.GetComponent<CMesh>().mesh, nullptr);
}

TEST_F(t_CMesh, DefaultSubmeshIndex_IsMinusOne)
{
    GameObject go = scene->CreateGameObject("Mesh");
    go.AddComponent<CMesh>();
    EXPECT_EQ(go.GetComponent<CMesh>().submeshIndex, -1);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CMesh, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Mesh");
    go.AddComponent<CMesh>();
    EXPECT_TRUE(go.HasComponent<CMesh>());
}

TEST_F(t_CMesh, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Mesh");
    go.AddComponent<CMesh>();
    CMesh& ref1 = go.GetComponent<CMesh>();
    CMesh& ref2 = go.GetComponent<CMesh>();
    EXPECT_EQ(&ref1, &ref2);
}

TEST_F(t_CMesh, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoMesh");
    EXPECT_EQ(go.TryGetComponent<CMesh>(), nullptr);
}

TEST_F(t_CMesh, TryGetComponent_NonNullWhenPresent)
{
    GameObject go = scene->CreateGameObject("WithMesh");
    go.AddComponent<CMesh>();
    EXPECT_NE(go.TryGetComponent<CMesh>(), nullptr);
}

TEST_F(t_CMesh, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("RemoveMesh");
    go.AddComponent<CMesh>();
    ASSERT_TRUE(go.HasComponent<CMesh>());
    go.RemoveComponent<CMesh>();
    EXPECT_FALSE(go.HasComponent<CMesh>());
}

TEST_F(t_CMesh, SubmeshIndex_CanBeSet)
{
    GameObject go = scene->CreateGameObject("SubMesh");
    go.AddComponent<CMesh>();
    go.GetComponent<CMesh>().submeshIndex = 2;
    EXPECT_EQ(go.GetComponent<CMesh>().submeshIndex, 2);
}

TEST_F(t_CMesh, MultipleGameObjects_IndependentMesh)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CMesh>();
    b.AddComponent<CMesh>();
    EXPECT_NE(&a.GetComponent<CMesh>(), &b.GetComponent<CMesh>());
}

// =============================================================================
// Resource resolution (requires the ComponentServices seam)
// =============================================================================

TEST_F(t_CMesh, Deserialize_WithSubmeshIndex_RequestsSubMeshThroughTheLoader)
{
    GameObject go = scene->CreateGameObject("Mesh");
    auto& m = go.AddComponent<CMesh>();

    JsonObject json;
    json.Set("assetPath",    "Assets/Models/cube.fbx");
    json.Set("submeshIndex", 0);
    m.Deserialize(json);

    EXPECT_TRUE(fakes.resources.Called("RequestOrCreateSubMeshResource"));
}

TEST_F(t_CMesh, Deserialize_WithLibraryPath_PrefersTheLibraryCall)
{
    GameObject go = scene->CreateGameObject("Mesh");
    auto& m = go.AddComponent<CMesh>();

    JsonObject json;
    json.Set("assetPath",    "Assets/Models/cube.fbx");
    json.Set("libraryPath",  "Library/Meshes/9.mesh");
    json.Set("resourceUID",  9.0);
    json.Set("submeshIndex", 2);
    m.Deserialize(json);

    // GAME path is tried first; the fake returns null, so the asset path is the fallback.
    EXPECT_TRUE(fakes.resources.Called("RequestOrCreateSubMeshResourceFromLibrary"));
    EXPECT_EQ(m.submeshIndex, 2);
}

TEST_F(t_CMesh, Deserialize_NoSubmeshIndex_UsesWholeResourceCall)
{
    GameObject go = scene->CreateGameObject("Mesh");
    auto& m = go.AddComponent<CMesh>();

    JsonObject json;
    json.Set("assetPath", "Assets/Models/cube.fbx");   // no submeshIndex key
    m.Deserialize(json);

    EXPECT_TRUE(fakes.resources.Called("CreateResource"));
    EXPECT_EQ(m.submeshIndex, -1);
}

TEST_F(t_CMesh, Deserialize_EmptyPaths_LeavesMeshNullAndCallsNothing)
{
    GameObject go = scene->CreateGameObject("Mesh");
    auto& m = go.AddComponent<CMesh>();

    JsonObject json;
    m.Deserialize(json);

    EXPECT_EQ(m.mesh, nullptr);
    EXPECT_TRUE(fakes.resources.calls.empty());
}

TEST_F(t_CMesh, Deserialize_UnwiredScene_DoesNotCrash)
{
    // No resource loader wired: the component must no-op, not dereference null.
    Scene* bare = NOUS_NEW<Scene>(MemoryTag::SCENE, "Bare");
    GameObject go = bare->CreateGameObject("Mesh");
    auto& m = go.AddComponent<CMesh>();

    JsonObject json;
    json.Set("assetPath",    "Assets/Models/cube.fbx");
    json.Set("submeshIndex", 0);
    m.Deserialize(json);

    EXPECT_EQ(m.mesh, nullptr);
    NOUS_DELETE(bare, MemoryTag::SCENE);
}

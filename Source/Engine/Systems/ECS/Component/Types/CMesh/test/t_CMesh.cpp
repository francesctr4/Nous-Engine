#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CMesh/include/CMesh.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

class t_CMesh : public ::testing::Test
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

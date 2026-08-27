#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CPrefab.h>
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"

class t_CPrefab : public ::testing::Test
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

TEST_F(t_CPrefab, DefaultPrefabSourcePath_IsEmpty)
{
    GameObject go = scene->CreateGameObject("Prefab");
    go.AddComponent<CPrefab>();
    EXPECT_TRUE(go.GetComponent<CPrefab>().prefabSourcePath.empty());
}

// =============================================================================
// Field mutation
// =============================================================================

TEST_F(t_CPrefab, PrefabSourcePath_CanBeSet)
{
    GameObject go = scene->CreateGameObject("Prefab");
    go.AddComponent<CPrefab>();
    go.GetComponent<CPrefab>().prefabSourcePath = "Assets/Prefabs/MyPrefab.nprefab";
    EXPECT_EQ(go.GetComponent<CPrefab>().prefabSourcePath, "Assets/Prefabs/MyPrefab.nprefab");
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CPrefab, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Prefab");
    go.AddComponent<CPrefab>();
    EXPECT_TRUE(go.HasComponent<CPrefab>());
}

TEST_F(t_CPrefab, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Prefab");
    go.AddComponent<CPrefab>();
    EXPECT_EQ(&go.GetComponent<CPrefab>(), &go.GetComponent<CPrefab>());
}

TEST_F(t_CPrefab, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoPrefab");
    EXPECT_EQ(go.TryGetComponent<CPrefab>(), nullptr);
}

TEST_F(t_CPrefab, TryGetComponent_NonNullWhenPresent)
{
    GameObject go = scene->CreateGameObject("Prefab");
    go.AddComponent<CPrefab>();
    EXPECT_NE(go.TryGetComponent<CPrefab>(), nullptr);
}

TEST_F(t_CPrefab, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("Prefab");
    go.AddComponent<CPrefab>();
    go.RemoveComponent<CPrefab>();
    EXPECT_FALSE(go.HasComponent<CPrefab>());
}

TEST_F(t_CPrefab, MultipleGameObjects_IndependentPath)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CPrefab>();
    b.AddComponent<CPrefab>();
    a.GetComponent<CPrefab>().prefabSourcePath = "Assets/Prefabs/A.nprefab";
    b.GetComponent<CPrefab>().prefabSourcePath = "Assets/Prefabs/B.nprefab";
    EXPECT_EQ(a.GetComponent<CPrefab>().prefabSourcePath, "Assets/Prefabs/A.nprefab");
    EXPECT_EQ(b.GetComponent<CPrefab>().prefabSourcePath, "Assets/Prefabs/B.nprefab");
}

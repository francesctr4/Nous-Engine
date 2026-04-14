#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

class t_Scene : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        MemoryManager::ShutdownMemory();
    }

    Scene* scene = nullptr;
};

// =============================================================================
// Basic scene state
// =============================================================================

TEST_F(t_Scene, NewScene_IsEmpty)
{
    EXPECT_EQ(scene->GetGameObjectsSnapshot().size(), 0u);
}

TEST_F(t_Scene, GetName_ReturnsConstructorName)
{
    EXPECT_EQ(scene->GetName(), "TestScene");
}

// =============================================================================
// CreateGameObject
// =============================================================================

TEST_F(t_Scene, CreateGameObject_IncreasesCount)
{
    scene->CreateGameObject("A");
    EXPECT_EQ(scene->GetGameObjectsSnapshot().size(), 1u);
}

TEST_F(t_Scene, CreateGameObject_ReturnsValidHandle)
{
    GameObject go = scene->CreateGameObject("Go");
    EXPECT_TRUE(go.IsValid());
}

TEST_F(t_Scene, CreateGameObject_HasCTransform)
{
    GameObject go = scene->CreateGameObject("CTransformCheck");
    EXPECT_TRUE(go.HasComponent<CTransform>());
}

TEST_F(t_Scene, CreateMultiple_EachHasUniqueID)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    GameObject c = scene->CreateGameObject("C");
    EXPECT_NE(a.GetID(), b.GetID());
    EXPECT_NE(b.GetID(), c.GetID());
    EXPECT_NE(a.GetID(), c.GetID());
}

// =============================================================================
// DestroyGameObject
// =============================================================================

TEST_F(t_Scene, DestroyGameObject_DecreasesCount)
{
    GameObject go = scene->CreateGameObject("Del");
    scene->DestroyGameObject(go);
    EXPECT_EQ(scene->GetGameObjectsSnapshot().size(), 0u);
}

TEST_F(t_Scene, DestroyGameObject_HandleBecomesInvalid)
{
    GameObject go = scene->CreateGameObject("Del2");
    scene->DestroyGameObject(go);
    EXPECT_FALSE(go.IsValid());
}

TEST_F(t_Scene, DestroyGameObject_AlsoDestroysChildren)
{
    GameObject parent = scene->CreateGameObject("Parent");
    scene->CreateGameObject("Child", &parent);
    ASSERT_EQ(scene->GetGameObjectsSnapshot().size(), 2u);

    scene->DestroyGameObject(parent);
    EXPECT_EQ(scene->GetGameObjectsSnapshot().size(), 0u);
}

// =============================================================================
// FindGameObjectByID
// =============================================================================

TEST_F(t_Scene, FindGameObjectByID_ReturnsCorrectGO)
{
    GameObject go = scene->CreateGameObject("FindMe");
    const uint32_t id = go.GetID();

    GameObject found = scene->FindGameObjectByID(id);
    EXPECT_EQ(found, go);
}

TEST_F(t_Scene, FindGameObjectByID_MissingID_ReturnsInvalid)
{
    GameObject found = scene->FindGameObjectByID(99999u);
    EXPECT_FALSE(found.IsValid());
}

TEST_F(t_Scene, IDMapSyncsOnDestroyCreate)
{
    GameObject go = scene->CreateGameObject("Temp");
    const uint32_t id = go.GetID();
    scene->DestroyGameObject(go);

    // The ID must not resolve after destruction — m_IDToEntity must be cleaned.
    EXPECT_FALSE(scene->FindGameObjectByID(id).IsValid());
}

// =============================================================================
// GetGameObjectsSnapshot
// =============================================================================

TEST_F(t_Scene, GetGameObjectsSnapshot_ContainsAllGOs)
{
    scene->CreateGameObject("A");
    scene->CreateGameObject("B");
    scene->CreateGameObject("C");
    EXPECT_EQ(scene->GetGameObjectsSnapshot().size(), 3u);
}

// =============================================================================
// Clear
// =============================================================================

TEST_F(t_Scene, Clear_RemovesAllGameObjects)
{
    scene->CreateGameObject("X");
    scene->CreateGameObject("Y");
    scene->Clear();
    EXPECT_EQ(scene->GetGameObjectsSnapshot().size(), 0u);
}

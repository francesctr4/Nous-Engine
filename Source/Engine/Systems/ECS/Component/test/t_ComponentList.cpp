#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Component/Types/ComponentTypes.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <string>
#include <vector>

class t_ComponentList : public ::testing::Test
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

// ── ForEachPresent ────────────────────────────────────────────────────────────

TEST_F(t_ComponentList, ForEachPresent_VisitsAttachedTypesInListOrder)
{
    GameObject go = scene->CreateGameObject("GO");
    go.AddComponent<CTransform>();
    go.AddComponent<CLight>();

    std::vector<std::string> seen;
    ComponentTypes::ForEachPresent(scene->GetRegistry(), go.GetEntity(),
        [&](Component* c) { seen.emplace_back(c->GetType()); });

    // CTransform precedes CLight in the ComponentTypes declaration order.
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], "CTransform");
    EXPECT_EQ(seen[1], "CLight");
}

TEST_F(t_ComponentList, ForEachPresent_SkipsAbsentTypes)
{
    GameObject go = scene->CreateGameObject("GO");
    go.AddComponent<CLight>();

    int count = 0;
    ComponentTypes::ForEachPresent(scene->GetRegistry(), go.GetEntity(),
        [&](Component*) { ++count; });

    // CreateGameObject auto-attaches CTransform, so the present set is
    // { CTransform, CLight } — proving the other 5 listed types are skipped.
    EXPECT_EQ(count, 2);
}

// ── AddByName ─────────────────────────────────────────────────────────────────

TEST_F(t_ComponentList, AddByName_KnownType_AttachesComponent)
{
    GameObject go = scene->CreateGameObject("GO");

    Component* c = ComponentTypes::AddByName(go, "CLight");

    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->GetType(), "CLight");
    EXPECT_TRUE(go.HasComponent<CLight>());
}

TEST_F(t_ComponentList, AddByName_UnknownType_ReturnsNull)
{
    GameObject go = scene->CreateGameObject("GO");
    EXPECT_EQ(ComponentTypes::AddByName(go, "CNonExistent"), nullptr);
}

// ── RemoveByName ──────────────────────────────────────────────────────────────

TEST_F(t_ComponentList, RemoveByName_KnownType_DetachesComponent)
{
    GameObject go = scene->CreateGameObject("GO");
    go.AddComponent<CLight>();

    EXPECT_TRUE(ComponentTypes::RemoveByName(go, "CLight"));
    EXPECT_FALSE(go.HasComponent<CLight>());
}

TEST_F(t_ComponentList, RemoveByName_UnknownType_ReturnsFalse)
{
    GameObject go = scene->CreateGameObject("GO");
    EXPECT_FALSE(ComponentTypes::RemoveByName(go, "CNonExistent"));
}

// ── ForEachType ───────────────────────────────────────────────────────────────

TEST_F(t_ComponentList, ForEachType_VisitsEveryDeclaredType)
{
    int typeCount = 0;
    ComponentTypes::ForEachType([&]<typename T>() { ++typeCount; });

    // CTransform, CMesh, CMaterial, CCamera, CLight, CScript, CPrefab, CAudioSource, CAudioListener, CVideoPlayer.
    EXPECT_EQ(typeCount, 10);
}

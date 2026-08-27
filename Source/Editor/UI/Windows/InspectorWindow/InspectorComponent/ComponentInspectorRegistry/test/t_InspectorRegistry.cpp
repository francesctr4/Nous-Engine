#include <gtest/gtest.h>

#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/ComponentInspectorRegistry/include/ComponentInspectorRegistry.h"
#include <ECS/Component/Types/ComponentTypes.h>
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"

#include <set>
#include <string>
#include <string_view>

class t_InspectorRegistry : public ::testing::Test
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

TEST_F(t_InspectorRegistry, FindComponentUI_KnownType_Found)
{
    const ComponentUI* e = FindComponentUI("CLight");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->typeName, "CLight");
}

TEST_F(t_InspectorRegistry, FindComponentUI_UnknownType_Null)
{
    EXPECT_EQ(FindComponentUI("CNonExistent"), nullptr);
}

TEST_F(t_InspectorRegistry, Table_HasNoDuplicateTypeNames)
{
    std::set<std::string_view> seen;
    for (const ComponentUI& e : ComponentUITable())
        EXPECT_TRUE(seen.insert(e.typeName).second)
            << "duplicate table entry: " << e.typeName;
}

// Drift guard: every table entry maps to a real engine component type. Catches typos
// and renames in the table that would otherwise silently drop a drawer. Uses a pure
// name check (no construction) so it doesn't run OnStart, which needs a wired engine.
TEST_F(t_InspectorRegistry, EveryTableEntry_MapsToRealComponent)
{
    for (const ComponentUI& e : ComponentUITable())
        EXPECT_TRUE(ComponentTypes::IsKnownName(e.typeName))
            << "table typeName has no matching engine component: " << e.typeName;
}

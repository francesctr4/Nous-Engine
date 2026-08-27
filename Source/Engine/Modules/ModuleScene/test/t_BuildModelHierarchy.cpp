#include <gtest/gtest.h>

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include <ECS/Scene/Scene.h>
#include <ECS/ECSInternalComponents.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <MemoryManager/MemoryManager.h>

#include <glm/glm.hpp>

#include <string>

// BuildModelHierarchyInto is pure structure-building: plain data in, entities out.
// No files, no assimp, no threads — which is exactly why it is worth testing.
class t_BuildModelHierarchy : public ::testing::Test
{
protected:
    static constexpr size_t kPoolSize = 8 * 1024 * 1024; // 8 MiB

    void SetUp() override    { nous::engine::memory::InitializeMemory(kPoolSize); }
    void TearDown() override { nous::engine::memory::ShutdownMemory(); }

    static PendingModelSpawn MakeSpawn(int submeshCount)
    {
        PendingModelSpawn spawn;
        spawn.rootName = "Model.fbx";

        for (int i = 0; i < submeshCount; ++i)
        {
            PendingSubMesh sub;
            sub.name         = "Sub" + std::to_string(i);
            sub.position     = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
            sub.submeshIndex = i;
            spawn.submeshes.push_back(std::move(sub));
        }
        return spawn;
    }
};

TEST_F(t_BuildModelHierarchy, CreatesRootPlusOneChildPerSubmesh)
{
    Scene scene;
    const size_t before = scene.GetGameObjects().size();

    BuildModelHierarchyInto(scene, MakeSpawn(3));

    EXPECT_EQ(scene.GetGameObjects().size(), before + 4); // 1 root + 3 children
}

TEST_F(t_BuildModelHierarchy, ChildrenAreParentedToTheRoot)
{
    Scene scene;
    BuildModelHierarchyInto(scene, MakeSpawn(2));

    GameObject root = scene.FindGameObjectByName("Model.fbx");
    ASSERT_TRUE(root.IsValid());

    // CHierarchy is an internal ECS component (it does not derive from Component),
    // so it is reached through the registry rather than TryGetComponent.
    const auto* hierarchy = scene.GetRegistry().try_get<CHierarchy>(root.GetEntity());
    ASSERT_NE(hierarchy, nullptr);
    EXPECT_EQ(hierarchy->children.size(), 2u);
}

TEST_F(t_BuildModelHierarchy, SubmeshTransformIsAppliedToTheChild)
{
    Scene scene;
    BuildModelHierarchyInto(scene, MakeSpawn(2));

    GameObject child = scene.FindGameObjectByName("Sub1");
    ASSERT_TRUE(child.IsValid());

    auto* t = child.TryGetComponent<CTransform>();
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 1.0f);
}

TEST_F(t_BuildModelHierarchy, EmptySpawnCreatesOnlyTheRoot)
{
    Scene scene;
    const size_t before = scene.GetGameObjects().size();

    BuildModelHierarchyInto(scene, MakeSpawn(0));

    EXPECT_EQ(scene.GetGameObjects().size(), before + 1);
}

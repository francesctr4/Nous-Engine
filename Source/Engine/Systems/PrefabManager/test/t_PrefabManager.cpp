#include <gtest/gtest.h>

#include "Engine/Systems/PrefabManager/include/PrefabManager.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include <glm/glm.hpp>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace
{
    const std::string kTempDir = (fs::temp_directory_path() / "nous_prefab_tests").string();

    std::string TempFile(const std::string& name)
    {
        return (fs::path(kTempDir) / name).string();
    }
}

// =============================================================================
// CPrefab — Serialization
// =============================================================================

class t_CPrefab : public ::testing::Test {};

TEST_F(t_CPrefab, Serialize_WritesType)
{
    CPrefab c;
    c.prefabSourcePath = "Assets/Prefabs/Test.nprefab";
    JsonObject val = c.Serialize();
    EXPECT_EQ(val.GetString("type"), "CPrefab");
}

TEST_F(t_CPrefab, Serialize_WritesSourcePath)
{
    CPrefab c;
    c.prefabSourcePath = "Assets/Prefabs/Test.nprefab";
    JsonObject val = c.Serialize();
    EXPECT_EQ(val.GetString("prefabSourcePath"), "Assets/Prefabs/Test.nprefab");
}

TEST_F(t_CPrefab, Deserialize_ReadsSourcePath)
{
    JsonObject obj;
    obj.Set("prefabSourcePath", std::string("Assets/Prefabs/Foo.nprefab"));

    CPrefab c;
    c.Deserialize(obj);
    EXPECT_EQ(c.prefabSourcePath, "Assets/Prefabs/Foo.nprefab");
}

TEST_F(t_CPrefab, Deserialize_MissingPath_DefaultsToEmpty)
{
    JsonObject obj;

    CPrefab c;
    c.prefabSourcePath = "old_value";
    c.Deserialize(obj);
    EXPECT_TRUE(c.prefabSourcePath.empty());
}

// =============================================================================
// PrefabManager — Fixture
// =============================================================================

class t_PrefabManager : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        fs::create_directories(kTempDir);
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        fs::remove_all(kTempDir);
        nous::engine::memory::ShutdownMemory();
    }

    // Creates root GO (+ optional child) in the fixture scene, saves to a temp
    // .nprefab file, and returns the file path.
    std::string SaveSimplePrefab(const std::string& filename,
                                  const std::string& rootName = "Root",
                                  bool               withChild = false,
                                  const std::string& childName = "Child")
    {
        GameObject root = scene->CreateGameObject(rootName, nullptr);
        if (withChild)
            scene->CreateGameObject(childName, &root);

        const std::string path = TempFile(filename);
        PrefabManager::SavePrefab(root, path);
        return path;
    }

    Scene* scene = nullptr;
};

// =============================================================================
// SavePrefab
// =============================================================================

TEST_F(t_PrefabManager, SavePrefab_InvalidRoot_DoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(PrefabManager::SavePrefab({}, TempFile("null.nprefab")));
}

TEST_F(t_PrefabManager, SavePrefab_CreatesFile)
{
    const std::string path = SaveSimplePrefab("save_creates_file.nprefab");
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(t_PrefabManager, SavePrefab_Version_IsOne)
{
    const std::string path = SaveSimplePrefab("save_version.nprefab");
    JsonObject root = JsonFile::LoadFromFile(path);
    ASSERT_FALSE(root.IsEmpty());
    const int version = static_cast<int>(root.GetDouble("version", 0.0));
    EXPECT_EQ(version, 1);
}

TEST_F(t_PrefabManager, SavePrefab_NameMatchesRootGO)
{
    const std::string path = SaveSimplePrefab("save_name.nprefab", "MyRoot");
    JsonObject root = JsonFile::LoadFromFile(path);
    ASSERT_FALSE(root.IsEmpty());
    EXPECT_EQ(root.GetString("name"), "MyRoot");
}

TEST_F(t_PrefabManager, SavePrefab_FileContainsGameObjectsArray)
{
    const std::string path = SaveSimplePrefab("save_has_array.nprefab");
    JsonObject root = JsonFile::LoadFromFile(path);
    ASSERT_FALSE(root.IsEmpty());
    EXPECT_FALSE(root.GetArray("GameObjects").IsEmpty());
}

TEST_F(t_PrefabManager, SavePrefab_RootEntryHasParentZero)
{
    const std::string path = SaveSimplePrefab("save_root_parent.nprefab");
    JsonObject root = JsonFile::LoadFromFile(path);
    ASSERT_FALSE(root.IsEmpty());
    JsonArray arr = root.GetArray("GameObjects");
    ASSERT_FALSE(arr.IsEmpty());

    bool foundRootEntry = false;
    for (int i = 0; i < arr.Count(); ++i)
    {
        JsonObject obj = arr.GetObject(i);
        if (static_cast<int>(obj.GetDouble("parent", -1.0)) == 0)
        {
            foundRootEntry = true;
            break;
        }
    }
    EXPECT_TRUE(foundRootEntry);
}

TEST_F(t_PrefabManager, SavePrefab_ChildEntryHasNonZeroParent)
{
    const std::string path = SaveSimplePrefab("save_child_parent.nprefab", "Root", true, "Child");
    JsonObject root = JsonFile::LoadFromFile(path);
    ASSERT_FALSE(root.IsEmpty());
    JsonArray arr = root.GetArray("GameObjects");
    ASSERT_FALSE(arr.IsEmpty());

    bool foundChild = false;
    for (int i = 0; i < arr.Count(); ++i)
    {
        JsonObject obj = arr.GetObject(i);
        if (static_cast<int>(obj.GetDouble("parent", 0.0)) != 0)
        {
            foundChild = true;
            break;
        }
    }
    EXPECT_TRUE(foundChild);
}

TEST_F(t_PrefabManager, SavePrefab_StripsCPrefabFromOutput)
{
    // Even if the root already carries a CPrefab, it must not appear in the saved file.
    GameObject root = scene->CreateGameObject("PrefabRoot", nullptr);
    auto& cp = root.AddComponent<CPrefab>();
    cp.prefabSourcePath = "old/path.nprefab";

    const std::string path = TempFile("save_strips_cprefab.nprefab");
    PrefabManager::SavePrefab(root, path);

    JsonObject fileRoot = JsonFile::LoadFromFile(path);
    ASSERT_FALSE(fileRoot.IsEmpty());
    JsonArray arr = fileRoot.GetArray("GameObjects");
    ASSERT_FALSE(arr.IsEmpty());

    bool hasCPrefab = false;
    for (int i = 0; i < arr.Count(); ++i)
    {
        JsonObject obj  = arr.GetObject(i);
        JsonArray comps = obj.GetArray("components");
        if (comps.IsEmpty()) continue;
        for (int j = 0; j < comps.Count(); ++j)
        {
            JsonObject comp = comps.GetObject(j);
            if (comp.GetString("type") == "CPrefab") { hasCPrefab = true; break; }
        }
        if (hasCPrefab) break;
    }

    EXPECT_FALSE(hasCPrefab);
}

// =============================================================================
// InstantiatePrefab
// =============================================================================

TEST_F(t_PrefabManager, InstantiatePrefab_NullScene_ReturnsInvalid)
{
    const std::string path = SaveSimplePrefab("inst_null_scene.nprefab");
    EXPECT_FALSE(PrefabManager::InstantiatePrefab(path, nullptr).IsValid());
}

TEST_F(t_PrefabManager, InstantiatePrefab_NonExistentFile_ReturnsInvalid)
{
    EXPECT_FALSE(PrefabManager::InstantiatePrefab("does_not_exist.nprefab", scene).IsValid());
}

TEST_F(t_PrefabManager, InstantiatePrefab_ValidFile_ReturnsValid)
{
    const std::string path = SaveSimplePrefab("inst_valid.nprefab");
    EXPECT_TRUE(PrefabManager::InstantiatePrefab(path, scene).IsValid());
}

TEST_F(t_PrefabManager, InstantiatePrefab_RootHasCPrefab)
{
    const std::string path = SaveSimplePrefab("inst_has_cprefab.nprefab");
    GameObject root = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(root.IsValid());
    EXPECT_TRUE(root.HasComponent<CPrefab>());
}

TEST_F(t_PrefabManager, InstantiatePrefab_CPrefabSourcePathMatchesFile)
{
    const std::string path = SaveSimplePrefab("inst_source_path.nprefab");
    GameObject root = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(root.IsValid());
    auto* cprefab = root.TryGetComponent<CPrefab>();
    ASSERT_NE(cprefab, nullptr);
    EXPECT_EQ(cprefab->prefabSourcePath, path);
}

TEST_F(t_PrefabManager, InstantiatePrefab_AllGOsRegisteredInScene)
{
    // SaveSimplePrefab adds root + 1 child to the fixture scene.
    // InstantiatePrefab must create 2 new GOs (copies) and register them — count grows by 2.
    const std::string path = SaveSimplePrefab("inst_registered.nprefab", "Root", true, "Child");
    const size_t countBefore = scene->GetGameObjectsSnapshot().size();

    PrefabManager::InstantiatePrefab(path, scene);

    EXPECT_EQ(scene->GetGameObjectsSnapshot().size() - countBefore, 2u);
}

TEST_F(t_PrefabManager, InstantiatePrefab_WithParent_RootIsChildOfParent)
{
    const std::string path = SaveSimplePrefab("inst_parent.nprefab");
    GameObject parent = scene->CreateGameObject("Parent", nullptr);

    GameObject root = PrefabManager::InstantiatePrefab(path, scene, parent);
    ASSERT_TRUE(root.IsValid());
    EXPECT_EQ(root.GetParent(), parent);
}

TEST_F(t_PrefabManager, InstantiatePrefab_NoParent_RootHasNoSceneParent)
{
    const std::string path = SaveSimplePrefab("inst_no_parent.nprefab");
    GameObject root = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(root.IsValid());
    EXPECT_FALSE(root.GetParent().IsValid());
}

// =============================================================================
// ReloadPrefabInstance
// =============================================================================

TEST_F(t_PrefabManager, ReloadPrefabInstance_NoCPrefab_DoesNotCrash)
{
    GameObject go = scene->CreateGameObject("NoPrefab", nullptr);
    EXPECT_NO_FATAL_FAILURE(PrefabManager::ReloadPrefabInstance(go, scene));
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_MissingSourceFile_DoesNotCrash)
{
    GameObject go = scene->CreateGameObject("Orphan", nullptr);
    auto& cprefab = go.AddComponent<CPrefab>();
    cprefab.prefabSourcePath = "does_not_exist.nprefab";
    EXPECT_NO_FATAL_FAILURE(PrefabManager::ReloadPrefabInstance(go, scene));
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_ChildrenAreRebuilt)
{
    // Prefab: Root + 1 child "ChildFromPrefab"
    const std::string path = SaveSimplePrefab("reload_children.nprefab", "Root", true, "ChildFromPrefab");

    GameObject instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(instanceRoot.IsValid());
    ASSERT_EQ(instanceRoot.GetChildren().size(), 1u);

    // Add an extra child that should not survive the reload.
    scene->CreateGameObject("ExtraChild", &instanceRoot);
    ASSERT_EQ(instanceRoot.GetChildren().size(), 2u);

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    // Only the prefab's original child must remain.
    EXPECT_EQ(instanceRoot.GetChildren().size(), 1u);
    EXPECT_EQ(instanceRoot.GetChildren()[0].GetName(), "ChildFromPrefab");
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_RootCTransformPreserved)
{
    const std::string path = SaveSimplePrefab("reload_transform.nprefab");

    GameObject instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(instanceRoot.IsValid());

    // Move the instance root before reloading.
    auto* t = instanceRoot.TryGetComponent<CTransform>();
    ASSERT_NE(t, nullptr);
    t->position = glm::vec3(10.f, 20.f, 30.f);
    t->UpdateMatrix();

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    // CTransform must still exist and position must be unchanged.
    auto* tAfter = instanceRoot.TryGetComponent<CTransform>();
    ASSERT_NE(tAfter, nullptr);
    EXPECT_FLOAT_EQ(tAfter->position.x, 10.f);
    EXPECT_FLOAT_EQ(tAfter->position.y, 20.f);
    EXPECT_FLOAT_EQ(tAfter->position.z, 30.f);
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_RootCPrefabPreserved)
{
    const std::string path = SaveSimplePrefab("reload_cprefab.nprefab");

    GameObject instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(instanceRoot.IsValid());

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    EXPECT_TRUE(instanceRoot.HasComponent<CPrefab>());
    EXPECT_EQ(instanceRoot.TryGetComponent<CPrefab>()->prefabSourcePath, path);
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_RootNameUpdatedFromFile)
{
    const std::string path = SaveSimplePrefab("reload_name.nprefab", "OriginalName");

    GameObject instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(instanceRoot.IsValid());

    instanceRoot.SetName("RenamedInScene");
    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    EXPECT_EQ(instanceRoot.GetName(), "OriginalName");
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_StaleComponentRemovedFromRoot)
{
    // Prefab root has only CTransform — no CCamera.
    const std::string path = SaveSimplePrefab("reload_stale_comp.nprefab", "StaleRoot");

    GameObject instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_TRUE(instanceRoot.IsValid());

    // Manually add CCamera to the instance root after instantiation.
    // This simulates a component that was removed from the prefab file since last save.
    instanceRoot.AddComponent<CCamera>();
    ASSERT_TRUE(instanceRoot.HasComponent<CCamera>());

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    // CCamera is not in the prefab file — it must be stripped.
    EXPECT_FALSE(instanceRoot.HasComponent<CCamera>());
    // CTransform and CPrefab must always survive the reload.
    EXPECT_TRUE(instanceRoot.HasComponent<CTransform>());
    EXPECT_TRUE(instanceRoot.HasComponent<CPrefab>());
}

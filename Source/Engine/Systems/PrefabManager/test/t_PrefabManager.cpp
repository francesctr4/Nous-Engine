#include <gtest/gtest.h>

#include "Engine/Systems/PrefabManager/include/PrefabManager.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

// Minimal test-only component — used to verify that stale components are stripped
// from a prefab instance root during ReloadPrefabInstance. Defined here to avoid
// depending on CMesh or any other component whose virtual methods aren't DLL-exported.
class CTestStale : public Component
{
public:
    COMPONENT_TYPE(CTestStale)
    JSON_Value* Serialize() const override { return nullptr; }
    void        Deserialize(JSON_Object*)  override {}
};

#include <parson.h>
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
    JSON_Value* val = c.Serialize();
    ASSERT_NE(val, nullptr);
    const char* type = json_object_get_string(json_value_get_object(val), "type");
    EXPECT_STREQ(type, "CPrefab");
    json_value_free(val);
}

TEST_F(t_CPrefab, Serialize_WritesSourcePath)
{
    CPrefab c;
    c.prefabSourcePath = "Assets/Prefabs/Test.nprefab";
    JSON_Value* val = c.Serialize();
    ASSERT_NE(val, nullptr);
    const char* path = json_object_get_string(json_value_get_object(val), "prefabSourcePath");
    EXPECT_STREQ(path, "Assets/Prefabs/Test.nprefab");
    json_value_free(val);
}

TEST_F(t_CPrefab, Deserialize_ReadsSourcePath)
{
    JSON_Value* val = json_value_init_object();
    JSON_Object* obj = json_value_get_object(val);
    json_object_set_string(obj, "prefabSourcePath", "Assets/Prefabs/Foo.nprefab");

    CPrefab c;
    c.Deserialize(obj);
    EXPECT_EQ(c.prefabSourcePath, "Assets/Prefabs/Foo.nprefab");
    json_value_free(val);
}

TEST_F(t_CPrefab, Deserialize_MissingPath_DefaultsToEmpty)
{
    JSON_Value* val = json_value_init_object();
    JSON_Object* obj = json_value_get_object(val);

    CPrefab c;
    c.prefabSourcePath = "old_value";
    c.Deserialize(obj);
    EXPECT_TRUE(c.prefabSourcePath.empty());
    json_value_free(val);
}

// =============================================================================
// PrefabManager — Fixture
// =============================================================================

class t_PrefabManager : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::InitializeMemory(MiB(16));
        fs::create_directories(kTempDir);
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        fs::remove_all(kTempDir);
        MemoryManager::ShutdownMemory();
    }

    // Creates root GO (+ optional child) in the fixture scene, saves to a temp
    // .nprefab file, and returns the file path.
    std::string SaveSimplePrefab(const std::string& filename,
                                  const std::string& rootName = "Root",
                                  bool               withChild = false,
                                  const std::string& childName = "Child")
    {
        GameObject* root = scene->CreateGameObject(rootName, nullptr);
        if (withChild)
            scene->CreateGameObject(childName, root);

        const std::string path = TempFile(filename);
        PrefabManager::SavePrefab(root, path);
        return path;
    }

    Scene* scene = nullptr;
};

// =============================================================================
// SavePrefab
// =============================================================================

TEST_F(t_PrefabManager, SavePrefab_NullRoot_DoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(PrefabManager::SavePrefab(nullptr, TempFile("null.nprefab")));
}

TEST_F(t_PrefabManager, SavePrefab_CreatesFile)
{
    const std::string path = SaveSimplePrefab("save_creates_file.nprefab");
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(t_PrefabManager, SavePrefab_Version_IsOne)
{
    const std::string path = SaveSimplePrefab("save_version.nprefab");
    JSON_Value* root = json_parse_file(path.c_str());
    ASSERT_NE(root, nullptr);
    const int version = static_cast<int>(json_object_get_number(json_value_get_object(root), "version"));
    EXPECT_EQ(version, 1);
    json_value_free(root);
}

TEST_F(t_PrefabManager, SavePrefab_NameMatchesRootGO)
{
    const std::string path = SaveSimplePrefab("save_name.nprefab", "MyRoot");
    JSON_Value* root = json_parse_file(path.c_str());
    ASSERT_NE(root, nullptr);
    const char* name = json_object_get_string(json_value_get_object(root), "name");
    EXPECT_STREQ(name, "MyRoot");
    json_value_free(root);
}

TEST_F(t_PrefabManager, SavePrefab_FileContainsGameObjectsArray)
{
    const std::string path = SaveSimplePrefab("save_has_array.nprefab");
    JSON_Value* root = json_parse_file(path.c_str());
    ASSERT_NE(root, nullptr);
    JSON_Array* arr = json_object_get_array(json_value_get_object(root), "GameObjects");
    EXPECT_NE(arr, nullptr);
    json_value_free(root);
}

TEST_F(t_PrefabManager, SavePrefab_RootEntryHasParentZero)
{
    const std::string path = SaveSimplePrefab("save_root_parent.nprefab");
    JSON_Value* root = json_parse_file(path.c_str());
    ASSERT_NE(root, nullptr);
    JSON_Array* arr = json_object_get_array(json_value_get_object(root), "GameObjects");
    ASSERT_NE(arr, nullptr);

    bool foundRootEntry = false;
    for (size_t i = 0; i < json_array_get_count(arr); ++i)
    {
        JSON_Object* obj = json_array_get_object(arr, i);
        if (static_cast<int>(json_object_get_number(obj, "parent")) == 0)
        {
            foundRootEntry = true;
            break;
        }
    }
    EXPECT_TRUE(foundRootEntry);
    json_value_free(root);
}

TEST_F(t_PrefabManager, SavePrefab_ChildEntryHasNonZeroParent)
{
    const std::string path = SaveSimplePrefab("save_child_parent.nprefab", "Root", true, "Child");
    JSON_Value* root = json_parse_file(path.c_str());
    ASSERT_NE(root, nullptr);
    JSON_Array* arr = json_object_get_array(json_value_get_object(root), "GameObjects");
    ASSERT_NE(arr, nullptr);

    bool foundChild = false;
    for (size_t i = 0; i < json_array_get_count(arr); ++i)
    {
        JSON_Object* obj = json_array_get_object(arr, i);
        if (static_cast<int>(json_object_get_number(obj, "parent")) != 0)
        {
            foundChild = true;
            break;
        }
    }
    EXPECT_TRUE(foundChild);
    json_value_free(root);
}

TEST_F(t_PrefabManager, SavePrefab_StripsCPrefabFromOutput)
{
    // Even if the root already carries a CPrefab, it must not appear in the saved file.
    GameObject* root = scene->CreateGameObject("PrefabRoot", nullptr);
    auto& cp = root->AddComponent<CPrefab>();
    cp.prefabSourcePath = "old/path.nprefab";

    const std::string path = TempFile("save_strips_cprefab.nprefab");
    PrefabManager::SavePrefab(root, path);

    JSON_Value* fileRoot = json_parse_file(path.c_str());
    ASSERT_NE(fileRoot, nullptr);
    JSON_Array* arr = json_object_get_array(json_value_get_object(fileRoot), "GameObjects");
    ASSERT_NE(arr, nullptr);

    bool hasCPrefab = false;
    for (size_t i = 0; i < json_array_get_count(arr); ++i)
    {
        JSON_Object* obj   = json_array_get_object(arr, i);
        JSON_Array*  comps = json_object_get_array(obj, "components");
        if (!comps) continue;
        for (size_t j = 0; j < json_array_get_count(comps); ++j)
        {
            const char* type = json_object_get_string(json_array_get_object(comps, j), "type");
            if (type && std::string(type) == "CPrefab") { hasCPrefab = true; break; }
        }
        if (hasCPrefab) break;
    }

    EXPECT_FALSE(hasCPrefab);
    json_value_free(fileRoot);
}

// =============================================================================
// InstantiatePrefab
// =============================================================================

TEST_F(t_PrefabManager, InstantiatePrefab_NullScene_ReturnsNull)
{
    const std::string path = SaveSimplePrefab("inst_null_scene.nprefab");
    EXPECT_EQ(PrefabManager::InstantiatePrefab(path, nullptr), nullptr);
}

TEST_F(t_PrefabManager, InstantiatePrefab_NonExistentFile_ReturnsNull)
{
    EXPECT_EQ(PrefabManager::InstantiatePrefab("does_not_exist.nprefab", scene), nullptr);
}

TEST_F(t_PrefabManager, InstantiatePrefab_ValidFile_ReturnsNonNull)
{
    const std::string path = SaveSimplePrefab("inst_valid.nprefab");
    EXPECT_NE(PrefabManager::InstantiatePrefab(path, scene), nullptr);
}

TEST_F(t_PrefabManager, InstantiatePrefab_RootHasCPrefab)
{
    const std::string path = SaveSimplePrefab("inst_has_cprefab.nprefab");
    GameObject* root = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->HasComponent<CPrefab>());
}

TEST_F(t_PrefabManager, InstantiatePrefab_CPrefabSourcePathMatchesFile)
{
    const std::string path = SaveSimplePrefab("inst_source_path.nprefab");
    GameObject* root = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(root, nullptr);
    auto* cprefab = root->TryGetComponent<CPrefab>();
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
    GameObject* parent = scene->CreateGameObject("Parent", nullptr);

    GameObject* root = PrefabManager::InstantiatePrefab(path, scene, parent);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->GetParent(), parent);
}

TEST_F(t_PrefabManager, InstantiatePrefab_NoParent_RootHasNoSceneParent)
{
    const std::string path = SaveSimplePrefab("inst_no_parent.nprefab");
    GameObject* root = PrefabManager::InstantiatePrefab(path, scene, nullptr);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->GetParent(), nullptr);
}

// =============================================================================
// ReloadPrefabInstance
// =============================================================================

TEST_F(t_PrefabManager, ReloadPrefabInstance_NoCPrefab_DoesNotCrash)
{
    GameObject* go = scene->CreateGameObject("NoPrefab", nullptr);
    EXPECT_NO_FATAL_FAILURE(PrefabManager::ReloadPrefabInstance(go, scene));
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_MissingSourceFile_DoesNotCrash)
{
    GameObject* go = scene->CreateGameObject("Orphan", nullptr);
    auto& cprefab = go->AddComponent<CPrefab>();
    cprefab.prefabSourcePath = "does_not_exist.nprefab";
    EXPECT_NO_FATAL_FAILURE(PrefabManager::ReloadPrefabInstance(go, scene));
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_ChildrenAreRebuilt)
{
    // Prefab: Root + 1 child "ChildFromPrefab"
    const std::string path = SaveSimplePrefab("reload_children.nprefab", "Root", true, "ChildFromPrefab");

    GameObject* instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(instanceRoot, nullptr);
    ASSERT_EQ(instanceRoot->GetChildren().size(), 1u);

    // Add an extra child that should not survive the reload.
    scene->CreateGameObject("ExtraChild", instanceRoot);
    ASSERT_EQ(instanceRoot->GetChildren().size(), 2u);

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    // Only the prefab's original child must remain.
    EXPECT_EQ(instanceRoot->GetChildren().size(), 1u);
    EXPECT_EQ(instanceRoot->GetChildren()[0]->GetName(), "ChildFromPrefab");
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_RootCTransformPreserved)
{
    const std::string path = SaveSimplePrefab("reload_transform.nprefab");

    GameObject* instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(instanceRoot, nullptr);

    // Move the instance root before reloading.
    auto* t = instanceRoot->TryGetComponent<CTransform>();
    ASSERT_NE(t, nullptr);
    t->position = glm::vec3(10.f, 20.f, 30.f);
    t->UpdateMatrix();

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    // CTransform must still exist and position must be unchanged.
    auto* tAfter = instanceRoot->TryGetComponent<CTransform>();
    ASSERT_NE(tAfter, nullptr);
    EXPECT_FLOAT_EQ(tAfter->position.x, 10.f);
    EXPECT_FLOAT_EQ(tAfter->position.y, 20.f);
    EXPECT_FLOAT_EQ(tAfter->position.z, 30.f);
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_RootCPrefabPreserved)
{
    const std::string path = SaveSimplePrefab("reload_cprefab.nprefab");

    GameObject* instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(instanceRoot, nullptr);

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    EXPECT_TRUE(instanceRoot->HasComponent<CPrefab>());
    EXPECT_EQ(instanceRoot->TryGetComponent<CPrefab>()->prefabSourcePath, path);
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_RootNameUpdatedFromFile)
{
    const std::string path = SaveSimplePrefab("reload_name.nprefab", "OriginalName");

    GameObject* instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(instanceRoot, nullptr);

    instanceRoot->SetName("RenamedInScene");
    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    EXPECT_EQ(instanceRoot->GetName(), "OriginalName");
}

TEST_F(t_PrefabManager, ReloadPrefabInstance_StaleComponentRemovedFromRoot)
{
    // Prefab root has only CTransform — no CTestStale.
    const std::string path = SaveSimplePrefab("reload_stale_comp.nprefab", "StaleRoot");

    GameObject* instanceRoot = PrefabManager::InstantiatePrefab(path, scene);
    ASSERT_NE(instanceRoot, nullptr);

    // Manually add CTestStale to the instance root after instantiation.
    // This simulates a component that was removed from the prefab file since last save.
    instanceRoot->AddComponent<CTestStale>();
    ASSERT_TRUE(instanceRoot->HasComponent<CTestStale>());

    PrefabManager::ReloadPrefabInstance(instanceRoot, scene);

    // CTestStale is not in the prefab file — it must be stripped.
    EXPECT_FALSE(instanceRoot->HasComponent<CTestStale>());
    // CTransform and CPrefab must always survive the reload.
    EXPECT_TRUE(instanceRoot->HasComponent<CTransform>());
    EXPECT_TRUE(instanceRoot->HasComponent<CPrefab>());
}

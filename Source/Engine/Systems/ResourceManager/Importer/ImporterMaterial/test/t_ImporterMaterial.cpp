#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include <parson.h>
#include <filesystem>
#include <fstream>
#include <string>

// ─── Fixture ──────────────────────────────────────────────────────────────────

class t_ImporterMaterial : public ::testing::Test
{
protected:
    static constexpr uint64 kMemoryPoolSize = MiB(16);

    std::string m_testFilePath;

    void SetUp() override
    {
        MemoryManager::InitializeMemory(kMemoryPoolSize);
        m_testFilePath = (std::filesystem::temp_directory_path() / "nous_t_ImporterMaterial.nmat").string();
    }

    void TearDown() override
    {
        std::filesystem::remove(m_testFilePath);
        MemoryManager::ShutdownMemory();
    }

    // Writes a minimal .nmat JSON string to the temp file.
    void WriteJson(const std::string& json) const
    {
        std::ofstream f(m_testFilePath);
        f << json;
    }
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_F(t_ImporterMaterial, CreateNewMaterialFileWritesValidJson)
{
    ASSERT_TRUE(ImporterMaterial::CreateNewMaterialFile(m_testFilePath));

    JSON_Value* root = json_parse_file(m_testFilePath.c_str());
    ASSERT_NE(root, nullptr);

    JSON_Object* obj = json_value_get_object(root);
    EXPECT_NE(json_object_get_array(obj, "uniforms"),      nullptr);
    EXPECT_NE(json_object_get_array(obj, "texture_maps"),  nullptr);

    json_value_free(root);
}

TEST_F(t_ImporterMaterial, CreateNewMaterialFileDeserializeRoundTrip)
{
    // CreateNewMaterialFile produces a file whose uniforms Deserialize can read back.
    ASSERT_TRUE(ImporterMaterial::CreateNewMaterialFile(m_testFilePath));

    ResourceMaterial mat;
    ImporterMaterial importer;
    // mResourceManager is null: safe because the file has no texture_maps entries
    // with valid UIDs and no shader_asset_path, so those code paths are not reached.
    ASSERT_TRUE(importer.Deserialize(m_testFilePath, &mat));

    // CreateNewMaterialFile seeds these six named uniforms.
    EXPECT_NE(mat.uniformValues.find("diffuseColor"),      mat.uniformValues.end());
    EXPECT_NE(mat.uniformValues.find("emissiveColor"),     mat.uniformValues.end());
    EXPECT_NE(mat.uniformValues.find("aoIntensity"),       mat.uniformValues.end());
    EXPECT_NE(mat.uniformValues.find("normalStrength"),    mat.uniformValues.end());
    EXPECT_NE(mat.uniformValues.find("specularIntensity"), mat.uniformValues.end());
    EXPECT_NE(mat.uniformValues.find("shininessScale"),    mat.uniformValues.end());
}

TEST_F(t_ImporterMaterial, DeserializeReadsVec4UniformCorrectly)
{
    WriteJson(R"({
        "uniforms": [
            { "name": "testColor", "type": "vec4", "value": [0.5, 0.25, 0.1, 1.0] }
        ]
    })");

    ResourceMaterial mat;
    ImporterMaterial importer;
    ASSERT_TRUE(importer.Deserialize(m_testFilePath, &mat));

    ASSERT_NE(mat.uniformValues.find("testColor"), mat.uniformValues.end());
    const UniformValue& uv = mat.uniformValues.at("testColor");
    EXPECT_FLOAT_EQ(uv.fdata.x, 0.5f);
    EXPECT_FLOAT_EQ(uv.fdata.y, 0.25f);
    EXPECT_FLOAT_EQ(uv.fdata.z, 0.1f);
    EXPECT_FLOAT_EQ(uv.fdata.w, 1.0f);
}

TEST_F(t_ImporterMaterial, DeserializeSkipsMalformedEntryMissingValueArray)
{
    // Entry has name and type but no "value" array — must be silently skipped.
    WriteJson(R"({
        "uniforms": [
            { "name": "broken", "type": "vec4" }
        ]
    })");

    ResourceMaterial mat;
    ImporterMaterial importer;
    ASSERT_TRUE(importer.Deserialize(m_testFilePath, &mat));

    EXPECT_TRUE(mat.uniformValues.empty());
}

TEST_F(t_ImporterMaterial, DeserializeReturnsFalseForMissingFile)
{
    ResourceMaterial mat;
    ImporterMaterial importer;
    EXPECT_FALSE(importer.Deserialize("__nonexistent__.nmat", &mat));
}

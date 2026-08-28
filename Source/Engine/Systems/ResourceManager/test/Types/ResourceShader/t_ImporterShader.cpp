#include <gtest/gtest.h>

#include "Types/ResourceShader/ImporterShader.h"
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <MemoryManager/MemoryManager.h>
#include <EngineCore/Globals.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// =============================================================================
// Helpers
// =============================================================================

namespace
{
    const std::string kTempDir =
        (fs::temp_directory_path() / "nous_t_ImporterShader").string();

    // Minimal vert+frag unified GLSL
    constexpr const char* kShaderGlsl = R"(#pragma stage vertex
#version 450
layout(location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos, 1.0); }
#pragma stage fragment
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(1.0); }
)";
}

// =============================================================================
// Fixture
// =============================================================================

class t_ImporterShader : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(32));
        fs::create_directories(kTempDir);
        m_glslPath    = (fs::path(kTempDir) / "test.glsl").string();
        m_libraryPath = (fs::path(kTempDir) / "test_shader.spv").string();

        std::ofstream f(m_glslPath);
        f << kShaderGlsl;
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(kTempDir, ec);
        nous::engine::memory::ShutdownMemory();
    }

    MetaFileData MakeMeta() const
    {
        MetaFileData meta;
        meta.name         = "TestShader";
        meta.uid          = 1;
        meta.resourceType = ResourceType::SHADER;
        meta.assetsPath   = m_glslPath;
        meta.libraryPath  = m_libraryPath;
        return meta;
    }

    std::string m_glslPath;
    std::string m_libraryPath;
};

// =============================================================================
// Deserialize — error cases
// =============================================================================

TEST_F(t_ImporterShader, Deserialize_MissingDirectory_ReturnsFalse)
{
    ResourceShader shader(1);
    ImporterShader importer;
    EXPECT_FALSE(importer.Deserialize("nonexistent/path/shader.spv", &shader));
}

TEST_F(t_ImporterShader, Deserialize_EmptyDirectory_ReturnsFalse)
{
    // Create the stage directory but put no .spv files in it
    const std::string emptyDir = (fs::path(kTempDir) / "empty_shader").string();
    fs::create_directories(emptyDir);

    // libraryPath has an extension so GetShaderDirectory strips it to emptyDir
    const std::string libraryPath = emptyDir + ".spv";

    ResourceShader shader(1);
    ImporterShader importer;
    EXPECT_FALSE(importer.Deserialize(libraryPath, &shader));
}

// =============================================================================
// Save → Deserialize roundtrip
// =============================================================================

TEST_F(t_ImporterShader, Save_ReturnsTrue_ForValidGlsl)
{
    MetaFileData meta = MakeMeta();
    ResourceBase* res = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    ImporterShader importer;
    EXPECT_TRUE(importer.Save(meta, res));
    // Save deletes and nulls res internally
}

TEST_F(t_ImporterShader, SaveThenDeserialize_PopulatesStagesData)
{
    MetaFileData meta = MakeMeta();
    ResourceBase* res = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    ImporterShader importer;
    ASSERT_TRUE(importer.Save(meta, res));

    ResourceShader shader(1);
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &shader));
    EXPECT_EQ(shader.stagesData.size(), 2u); // vertex + fragment
}

TEST_F(t_ImporterShader, SaveThenDeserialize_EachStageHasSpirV)
{
    MetaFileData meta = MakeMeta();
    ResourceBase* res = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    ImporterShader importer;
    ASSERT_TRUE(importer.Save(meta, res));

    ResourceShader shader(1);
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &shader));
    for (const ShaderSource& src : shader.stagesData)
        EXPECT_FALSE(src.spirvBinary.empty()) << "Stage " << static_cast<int>(src.stage);
}

TEST_F(t_ImporterShader, SaveThenDeserialize_ReflectionIsPopulated)
{
    MetaFileData meta = MakeMeta();
    ResourceBase* res = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    ImporterShader importer;
    ASSERT_TRUE(importer.Save(meta, res));

    ResourceShader shader(1);
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &shader));
    // At least one vertex input (aPos at location 0)
    EXPECT_GE(shader.reflection.vertexInputs.size(), 1u);
}

// =============================================================================
// Evict
// =============================================================================

TEST_F(t_ImporterShader, Evict_ClearsStagesDataAndReflection)
{
    MetaFileData meta = MakeMeta();
    ResourceBase* res = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    ImporterShader importer;
    ASSERT_TRUE(importer.Save(meta, res));

    ResourceShader shader(1);
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &shader));
    ASSERT_FALSE(shader.stagesData.empty());

    importer.Evict(&shader);
    EXPECT_TRUE(shader.stagesData.empty());
}

#include <gtest/gtest.h>

#include <ResourceManager/Import/ModelParser/ModelImportData.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using nous::engine::resource_manager::ModelImportData;

// These tests write files through ImporterMesh itself rather than hand-rolling the
// byte layout. The previous version hand-wrote "legacy" binaries using the LIVE
// sizeof(Vertex3D) — which silently stopped describing the on-disk format the
// moment Vertex3D gained bone data, and would have kept passing while testing
// nothing. Round-tripping through the writer cannot drift from the reader.

namespace
{
    SubMeshData MakeSubmesh(const std::string& name, uint32_t vertexCount,
                            const std::string& materialPath = {})
    {
        SubMeshData sub;
        sub.name              = name;
        sub.materialAssetPath = materialPath;
        sub.localTransform    = glm::mat4(1.0f);

        sub.vertices.resize(vertexCount);
        for (uint32_t i = 0; i < vertexCount; ++i)
            sub.vertices[i].position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);

        for (uint32_t i = 0; i < vertexCount; ++i) sub.indices.push_back(i);

        return sub;
    }
}

class t_ImporterMesh : public ::testing::Test
{
protected:
    std::string  m_testFilePath;
    MetaFileData m_meta;

    void SetUp() override
    {
        m_testFilePath = (std::filesystem::temp_directory_path() / "nous_t_ImporterMesh.nmesh").string();
        m_meta.assetsPath  = "Assets/Meshes/test.fbx";
        m_meta.libraryPath = m_testFilePath;
    }

    void TearDown() override
    {
        std::filesystem::remove(m_testFilePath);
    }

    void Write(std::vector<SubMeshData> submeshes)
    {
        ModelImportData model;
        model.submeshes = std::move(submeshes);
        ASSERT_TRUE(ImporterMesh::SaveModel(m_meta, model));
    }
};

// ─── Failure paths ────────────────────────────────────────────────────────────

TEST_F(t_ImporterMesh, LoadHierarchyReturnsEmptyOnMissingFile)
{
    EXPECT_TRUE(ImporterMesh::LoadHierarchy("__nonexistent__.nmesh").empty());
}

TEST_F(t_ImporterMesh, LoadSubmeshInfoReturnsEmptyOnMissingFile)
{
    EXPECT_TRUE(ImporterMesh::LoadSubmeshInfo("__nonexistent__.nmesh").empty());
}

TEST_F(t_ImporterMesh, LoadSubmeshReturnsFalseOnMissingFile)
{
    SubMeshData out;
    EXPECT_FALSE(ImporterMesh::LoadSubmesh("__nonexistent__.nmesh", 0, out));
}

TEST_F(t_ImporterMesh, DeserializeReturnsFalseOnMissingFile)
{
    ResourceMesh mesh;
    ImporterMesh importer;
    EXPECT_FALSE(importer.Deserialize("__nonexistent__.nmesh", &mesh));
}

// There is no back-compat read path by design, so a binary from an older engine
// must be REJECTED rather than misparsed — a wrong magic read as the current
// layout would produce plausible-looking garbage geometry.
TEST_F(t_ImporterMesh, BinaryWithUnknownMagicIsRejected)
{
    {
        std::ofstream f(m_testFilePath, std::ios::binary);
        const uint32_t staleMagic = 0xFA7C0DE1u;   // an older format's magic
        const uint32_t count      = 1;
        f.write(reinterpret_cast<const char*>(&staleMagic), 4);
        f.write(reinterpret_cast<const char*>(&count), 4);
    }

    EXPECT_TRUE(ImporterMesh::LoadHierarchy(m_testFilePath).empty());
    EXPECT_TRUE(ImporterMesh::LoadSubmeshInfo(m_testFilePath).empty());

    SubMeshData out;
    EXPECT_FALSE(ImporterMesh::LoadSubmesh(m_testFilePath, 0, out));
}

TEST_F(t_ImporterMesh, SaveModelRejectsAModelWithNoSubmeshes)
{
    const ModelImportData empty;
    EXPECT_FALSE(ImporterMesh::SaveModel(m_meta, empty));
}

// ─── Round trip ───────────────────────────────────────────────────────────────

TEST_F(t_ImporterMesh, RoundTripsEverySubmesh)
{
    Write({ MakeSubmesh("SubA", 3), MakeSubmesh("SubB", 4) });

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].name, "SubA");
    EXPECT_EQ(result[1].name, "SubB");
    EXPECT_EQ(result[0].vertices.size(), 3u);
    EXPECT_EQ(result[1].vertices.size(), 4u);
    EXPECT_EQ(result[0].indices.size(),  3u);
    EXPECT_EQ(result[1].indices.size(),  4u);
}

TEST_F(t_ImporterMesh, RoundTripsMaterialPathAndTransform)
{
    SubMeshData sub = MakeSubmesh("Sub", 1, "Assets/Meshes/test_Body.nmat");
    sub.localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));

    Write({ sub });

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].materialAssetPath, "Assets/Meshes/test_Body.nmat");
    EXPECT_EQ(glm::vec3(result[0].localTransform[3]), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(t_ImporterMesh, EmptyMaterialPathRoundTripsAsEmpty)
{
    Write({ MakeSubmesh("Sub", 1) });

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_TRUE(result[0].materialAssetPath.empty());
}

// Skinning data travels in the vertex array like everything else. If the writer and
// reader ever disagree on Vertex3D's size this is what catches it.
TEST_F(t_ImporterMesh, RoundTripsBoneIDsAndWeights)
{
    SubMeshData sub = MakeSubmesh("Skinned", 2);
    sub.vertices[0].boneIDs     = { 3u, 7u, 0u, 0u };
    sub.vertices[0].boneWeights = { 0.6f, 0.4f, 0.0f, 0.0f };
    sub.vertices[1].boneIDs     = { 1u, 0u, 0u, 0u };
    sub.vertices[1].boneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };

    Write({ sub });

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);

    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].vertices.size(), 2u);
    EXPECT_EQ(result[0].vertices[0].boneIDs,     glm::uvec4(3u, 7u, 0u, 0u));
    EXPECT_EQ(result[0].vertices[0].boneWeights, glm::vec4(0.6f, 0.4f, 0.0f, 0.0f));
    EXPECT_EQ(result[0].vertices[1].boneIDs,     glm::uvec4(1u, 0u, 0u, 0u));
}

TEST_F(t_ImporterMesh, UnskinnedVerticesRoundTripAsZeroInfluence)
{
    Write({ MakeSubmesh("Static", 2) });

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);

    ASSERT_EQ(result.size(), 1u);
    for (const Vertex3D& v : result[0].vertices)
    {
        EXPECT_EQ(v.boneIDs,     glm::uvec4(0u));
        EXPECT_EQ(v.boneWeights, glm::vec4(0.0f));
    }
}

// ─── Directory-backed readers ─────────────────────────────────────────────────

TEST_F(t_ImporterMesh, LoadSubmeshInfoReturnsHeadersWithoutGeometry)
{
    Write({ MakeSubmesh("SubA", 3, "a.nmat"), MakeSubmesh("SubB", 4, "b.nmat") });

    const auto info = ImporterMesh::LoadSubmeshInfo(m_testFilePath);

    ASSERT_EQ(info.size(), 2u);
    EXPECT_EQ(info[0].name, "SubA");
    EXPECT_EQ(info[1].name, "SubB");
    EXPECT_EQ(info[0].materialAssetPath, "a.nmat");
    EXPECT_EQ(info[1].materialAssetPath, "b.nmat");
}

TEST_F(t_ImporterMesh, LoadSubmeshReadsTheRequestedSubmeshOnly)
{
    Write({ MakeSubmesh("SubA", 3), MakeSubmesh("SubB", 4), MakeSubmesh("SubC", 5) });

    SubMeshData out;

    ASSERT_TRUE(ImporterMesh::LoadSubmesh(m_testFilePath, 1, out));
    EXPECT_EQ(out.name, "SubB");
    EXPECT_EQ(out.vertices.size(), 4u);

    // Seeking backwards must work too — the directory is random access, and a
    // stream left at EOF by a previous read would refuse to seek without a clear().
    ASSERT_TRUE(ImporterMesh::LoadSubmesh(m_testFilePath, 0, out));
    EXPECT_EQ(out.name, "SubA");
    EXPECT_EQ(out.vertices.size(), 3u);

    ASSERT_TRUE(ImporterMesh::LoadSubmesh(m_testFilePath, 2, out));
    EXPECT_EQ(out.name, "SubC");
    EXPECT_EQ(out.vertices.size(), 5u);
}

TEST_F(t_ImporterMesh, LoadSubmeshAgreesWithLoadHierarchy)
{
    Write({ MakeSubmesh("SubA", 3), MakeSubmesh("SubB", 7, "b.nmat") });

    const auto all = ImporterMesh::LoadHierarchy(m_testFilePath);
    ASSERT_EQ(all.size(), 2u);

    for (int32_t i = 0; i < 2; ++i)
    {
        SubMeshData one;
        ASSERT_TRUE(ImporterMesh::LoadSubmesh(m_testFilePath, i, one));

        const SubMeshData& expected = all[static_cast<size_t>(i)];
        EXPECT_EQ(one.name,              expected.name);
        EXPECT_EQ(one.materialAssetPath, expected.materialAssetPath);
        EXPECT_EQ(one.vertices.size(),   expected.vertices.size());
        EXPECT_EQ(one.indices.size(),    expected.indices.size());
    }
}

TEST_F(t_ImporterMesh, LoadSubmeshRejectsOutOfRangeIndices)
{
    Write({ MakeSubmesh("Only", 1) });

    SubMeshData out;
    EXPECT_FALSE(ImporterMesh::LoadSubmesh(m_testFilePath, 1, out));
    EXPECT_FALSE(ImporterMesh::LoadSubmesh(m_testFilePath, -1, out));
}

// ─── Deserialize ──────────────────────────────────────────────────────────────

TEST_F(t_ImporterMesh, DeserializeMergesAllSubmeshesIntoSingleMesh)
{
    // Sub 0: 2 vertices, indices [0, 1]
    // Sub 1: 3 vertices, indices [0, 1, 2]
    // After merge: 5 vertices; sub-1 indices offset by 2 -> [2, 3, 4]
    Write({ MakeSubmesh("Sub0", 2), MakeSubmesh("Sub1", 3) });

    ResourceMesh mesh;
    ImporterMesh importer;
    ASSERT_TRUE(importer.Deserialize(m_testFilePath, &mesh));

    EXPECT_EQ(mesh.vertices.size(), 5u);
    EXPECT_EQ(mesh.indices.size(),  5u);

    EXPECT_EQ(mesh.indices[2], 2u);
    EXPECT_EQ(mesh.indices[3], 3u);
    EXPECT_EQ(mesh.indices[4], 4u);
}

#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

// ─── Binary format constants (mirrors ImporterMesh.cpp) ───────────────────────
static constexpr uint32_t k_MeshBinaryMagic = 0xFA7C0DE1u;

// ─── Write helpers ────────────────────────────────────────────────────────────

static void WriteU32(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }
static void WriteU64(std::ofstream& f, uint64_t v) { f.write(reinterpret_cast<const char*>(&v), 8); }

// Writes a V1 legacy binary: uint64 vertCount | Vertex3D[] | uint64 idxCount | uint32[]
static void WriteV1Binary(const std::string& path,
                          const std::vector<Vertex3D>& verts,
                          const std::vector<uint32_t>& indices)
{
    std::ofstream f(path, std::ios::binary);
    WriteU64(f, static_cast<uint64_t>(verts.size()));
    f.write(reinterpret_cast<const char*>(verts.data()), verts.size() * sizeof(Vertex3D));
    WriteU64(f, static_cast<uint64_t>(indices.size()));
    f.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));
}

// Writes a V2 multi-submesh binary (current format).
static void WriteV2Binary(const std::string& path,
                          const std::vector<SubMeshData>& submeshes)
{
    std::ofstream f(path, std::ios::binary);
    WriteU32(f, k_MeshBinaryMagic);
    WriteU32(f, static_cast<uint32_t>(submeshes.size()));

    for (const auto& sub : submeshes)
    {
        const uint64_t nameLen = sub.name.size();
        WriteU64(f, nameLen);
        if (nameLen > 0)
            f.write(sub.name.data(), static_cast<std::streamsize>(nameLen));

        f.write(reinterpret_cast<const char*>(&sub.localTransform[0][0]), 16 * sizeof(float));

        WriteU64(f, static_cast<uint64_t>(sub.vertices.size()));
        if (!sub.vertices.empty())
            f.write(reinterpret_cast<const char*>(sub.vertices.data()),
                    sub.vertices.size() * sizeof(Vertex3D));

        WriteU64(f, static_cast<uint64_t>(sub.indices.size()));
        if (!sub.indices.empty())
            f.write(reinterpret_cast<const char*>(sub.indices.data()),
                    sub.indices.size() * sizeof(uint32_t));
    }
}

// ─── Fixture ──────────────────────────────────────────────────────────────────

class t_ImporterMesh : public ::testing::Test
{
protected:
    std::string m_testFilePath;

    void SetUp() override
    {
        m_testFilePath = (std::filesystem::temp_directory_path() / "nous_t_ImporterMesh.nmesh").string();
    }

    void TearDown() override
    {
        std::filesystem::remove(m_testFilePath);
    }
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_F(t_ImporterMesh, LoadHierarchyReturnsEmptyOnMissingFile)
{
    const auto result = ImporterMesh::LoadHierarchy("__nonexistent__.nmesh");
    EXPECT_TRUE(result.empty());
}

TEST_F(t_ImporterMesh, LoadHierarchyReadsSingleSubmeshFromV1Binary)
{
    Vertex3D v{};
    v.position = { 1.f, 2.f, 3.f };
    const std::vector<Vertex3D>  verts   = { v, v, v };
    const std::vector<uint32_t>  indices = { 0, 1, 2 };

    WriteV1Binary(m_testFilePath, verts, indices);

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].vertices.size(), 3u);
    EXPECT_EQ(result[0].indices.size(),  3u);
}

TEST_F(t_ImporterMesh, LoadHierarchyReadsTwoSubmeshesFromV2Binary)
{
    SubMeshData sub1, sub2;
    sub1.name = "SubA";
    sub1.localTransform = glm::mat4(1.0f);
    sub1.vertices.resize(3);
    sub1.indices = { 0, 1, 2 };

    sub2.name = "SubB";
    sub2.localTransform = glm::mat4(1.0f);
    sub2.vertices.resize(4);
    sub2.indices = { 0, 1, 2, 0, 2, 3 };

    WriteV2Binary(m_testFilePath, { sub1, sub2 });

    const auto result = ImporterMesh::LoadHierarchy(m_testFilePath);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].vertices.size(), 3u);
    EXPECT_EQ(result[1].vertices.size(), 4u);
    EXPECT_EQ(result[0].name, "SubA");
    EXPECT_EQ(result[1].name, "SubB");
}

TEST_F(t_ImporterMesh, DeserializeReturnsFalseOnMissingFile)
{
    ResourceMesh mesh;
    ImporterMesh importer;
    EXPECT_FALSE(importer.Deserialize("__nonexistent__.nmesh", &mesh));
}

TEST_F(t_ImporterMesh, DeserializeMergesAllV2SubmeshesIntoSingleMesh)
{
    // Sub 0: 2 vertices, indices [0, 1]
    // Sub 1: 3 vertices, indices [0, 1, 2]
    // After merge: 5 vertices; sub-1 indices offset by 2 → [2, 3, 4]
    SubMeshData sub0, sub1;
    sub0.localTransform = glm::mat4(1.0f);
    sub0.vertices.resize(2);
    sub0.indices = { 0, 1 };

    sub1.localTransform = glm::mat4(1.0f);
    sub1.vertices.resize(3);
    sub1.indices = { 0, 1, 2 };

    WriteV2Binary(m_testFilePath, { sub0, sub1 });

    ResourceMesh mesh;
    ImporterMesh importer;
    ASSERT_TRUE(importer.Deserialize(m_testFilePath, &mesh));

    EXPECT_EQ(mesh.vertices.size(), 5u);
    EXPECT_EQ(mesh.indices.size(),  5u);  // 2 + 3

    // Sub-1 indices must be re-based on sub-0's vertex count (2).
    EXPECT_EQ(mesh.indices[2], 2u);  // sub-1 index 0 + 2
    EXPECT_EQ(mesh.indices[3], 3u);  // sub-1 index 1 + 2
    EXPECT_EQ(mesh.indices[4], 4u);  // sub-1 index 2 + 2
}

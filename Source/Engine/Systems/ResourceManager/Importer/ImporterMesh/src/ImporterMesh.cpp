#include "Engine/Systems/ResourceManager/Importer/ImporterMesh/include/ImporterMesh.h"
#include "Engine/Core/FileSystem/FileHandle/include/FileHandle.h"

#include <map>

#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"

#include "Engine/Core/Logger/Logger.h"

// Assimp
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace)
#include "assimp/scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"

// ─── Binary format ────────────────────────────────────────────────────────────
// V1 (legacy): uint64 vertexCount | Vertex3D[] | uint64 indexCount | uint32[]
// V2 (current): uint32 magic | uint32 submeshCount | N×(nameLen:u64, name:chars,
//               localTransform:16×float, vertexCount:u64, Vertex3D[],
//               indexCount:u64, uint32[])
static constexpr uint32_t MESH_BINARY_MAGIC = 0xFA7C0DE1u;

// ─── Assimp helpers ───────────────────────────────────────────────────────────

static glm::mat4 AiToGlm(const aiMatrix4x4& m)
{
    // Assimp is row-major; GLM is column-major.
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,   // column 0
        m.a2, m.b2, m.c2, m.d2,   // column 1
        m.a3, m.b3, m.c3, m.d3,   // column 2
        m.a4, m.b4, m.c4, m.d4    // column 3
    );
}

// Weld smooth normals per position for the outline pass.
// For each vertex, accumulates and averages the face normals of all vertices that
// share the same position, then normalizes the result into smoothNormal.
static void WeldSmoothNormals(SubMeshData& out, size_t startIdx)
{
    struct Vec3Less {
        bool operator()(const glm::vec3& a, const glm::vec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        }
    };
    std::map<glm::vec3, std::pair<glm::vec3, uint32_t>, Vec3Less> accum;
    for (size_t i = startIdx; i < out.vertices.size(); ++i) {
        auto& [sum, cnt] = accum[out.vertices[i].position];
        sum += out.vertices[i].normal;
        ++cnt;
    }
    for (size_t i = startIdx; i < out.vertices.size(); ++i) {
        const auto& [sum, cnt] = accum[out.vertices[i].position];
        out.vertices[i].smoothNormal = glm::normalize(sum / static_cast<float>(cnt));
    }
}

// Fill one SubMeshData from an aiMesh.  smoothNormals are welded per position.
static void ExtractSubMesh(aiMesh* mesh, const glm::mat4& transform,
                            const std::string& name, SubMeshData& out)
{
    out.name           = name;
    out.localTransform = transform;

    const size_t startIdx = out.vertices.size();

    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex3D vertex;

        vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        vertex.normal   = { mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z  };

        vertex.color = mesh->HasVertexColors(0)
            ? glm::vec3(mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b)
            : glm::vec3(1.0f);

        vertex.texCoord = mesh->HasTextureCoords(0)
            ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
            : glm::vec2(0.0f);

        vertex.smoothNormal = { 0.0f, 0.0f, 0.0f }; // computed below

        if (mesh->HasTangentsAndBitangents())
        {
            const glm::vec3 t = { mesh->mTangents[i].x,   mesh->mTangents[i].y,   mesh->mTangents[i].z   };
            const glm::vec3 b = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
            const float sign  = (glm::dot(glm::cross(vertex.normal, t), b) < 0.0f) ? -1.0f : 1.0f;
            vertex.tangent    = glm::vec4(t, sign);
        }
        else
        {
            vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
        }

        vertex.texCoord2 = mesh->HasTextureCoords(1)
            ? glm::vec2(mesh->mTextureCoords[1][i].x, mesh->mTextureCoords[1][i].y)
            : glm::vec2(0.0f);

        out.vertices.emplace_back(vertex);
    }

    WeldSmoothNormals(out, startIdx);

    if (mesh->HasFaces())
    {
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
                out.indices.push_back(static_cast<uint32_t>(face.mIndices[j]));
        }
    }
}

// Recursively traverse nodes, accumulating the world transform.
// Each (node, meshIndex) pair becomes one SubMeshData.
static void CollectSubMeshData(aiNode* node, const aiScene* scene,
                                const glm::mat4& parentTransform,
                                std::vector<SubMeshData>& outSubmeshes)
{
    const glm::mat4 nodeTransform = parentTransform * AiToGlm(node->mTransformation);

    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        std::string name = mesh->mName.length > 0
            ? std::string(mesh->mName.C_Str())
            : (std::string(node->mName.C_Str()) + "_" + std::to_string(i));

        SubMeshData sub;
        ExtractSubMesh(mesh, nodeTransform, name, sub);
        outSubmeshes.emplace_back(std::move(sub));
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
        CollectSubMeshData(node->mChildren[i], scene, nodeTransform, outSubmeshes);
}

// ─── Write helpers ────────────────────────────────────────────────────────────

static bool WriteU32(FileHandle& fh, uint32_t v)
{
    uint64_t w = 0; return fh.Write(4, &v, &w);
}
static bool WriteU64(FileHandle& fh, uint64_t v)
{
    uint64_t w = 0; return fh.Write(8, &v, &w);
}
static bool WriteBytes(FileHandle& fh, const void* data, uint64_t size)
{
    uint64_t w = 0; return fh.Write(size, data, &w);
}

// Write a V2 multi-submesh binary from a pre-collected vector of SubMeshData.
static bool SaveSubmeshes(const MetaFileData& metaFileData,
                          const std::vector<SubMeshData>& submeshes)
{
    FileHandle fh;
    if (!fh.Open(metaFileData.libraryPath, FileMode::WRITE, true))
        return false;

    bool ok = true;
    ok &= WriteU32(fh, MESH_BINARY_MAGIC);
    ok &= WriteU32(fh, static_cast<uint32_t>(submeshes.size()));

    for (const auto& sub : submeshes)
    {
        // Name
        const uint64_t nameLen = sub.name.size();
        ok &= WriteU64(fh, nameLen);
        if (nameLen > 0)
            ok &= WriteBytes(fh, sub.name.data(), nameLen);

        // Local transform (16 floats, column-major)
        ok &= WriteBytes(fh, &sub.localTransform[0][0], 16 * sizeof(float));

        // Vertices
        const uint64_t vCount = sub.vertices.size();
        ok &= WriteU64(fh, vCount);
        if (vCount > 0)
            ok &= WriteBytes(fh, sub.vertices.data(), vCount * sizeof(Vertex3D));

        // Indices
        const uint64_t iCount = sub.indices.size();
        ok &= WriteU64(fh, iCount);
        if (iCount > 0)
            ok &= WriteBytes(fh, sub.indices.data(), iCount * sizeof(uint32_t));
    }

    fh.Close();
    return ok;
}

// ─── Binary parser ────────────────────────────────────────────────────────────
// Shared by Deserialize (which merges submeshes) and LoadHierarchy (which keeps them separate).
// Returns one SubMeshData per logical submesh; empty on open/read failure.

static std::vector<SubMeshData> ParseMeshBinary(const std::string& libraryPath)
{
    std::vector<SubMeshData> result;

    FileHandle fh;
    if (!fh.Open(libraryPath, FileMode::READ, true))
    {
        NOUS_ERROR("ParseMeshBinary: failed to open '%s'", libraryPath.c_str());
        return result;
    }

    uint64_t bytesRead = 0;

    uint32_t header4 = 0;
    if (!fh.ReadBytes(4, reinterpret_cast<char*>(&header4), &bytesRead))
        return result;

    if (header4 != MESH_BINARY_MAGIC)
    {
        // V1 format: single flat mesh — wrap in one SubMeshData
        uint32_t header4High = 0;
        fh.ReadBytes(4, reinterpret_cast<char*>(&header4High), &bytesRead);
        const uint64_t vCount = header4 | (static_cast<uint64_t>(header4High) << 32);

        SubMeshData sub;
        sub.name           = "Mesh";
        sub.localTransform = glm::mat4(1.0f);
        sub.vertices.resize(vCount);
        fh.ReadBytes(vCount * sizeof(Vertex3D), reinterpret_cast<char*>(sub.vertices.data()), &bytesRead);

        uint64_t iCount = 0;
        fh.ReadBytes(sizeof(iCount), reinterpret_cast<char*>(&iCount), &bytesRead);
        sub.indices.resize(iCount);
        fh.ReadBytes(iCount * sizeof(uint32_t), reinterpret_cast<char*>(sub.indices.data()), &bytesRead);

        fh.Close();
        result.emplace_back(std::move(sub));
        return result;
    }

    // V2 format
    uint32_t submeshCount = 0;
    if (!fh.ReadBytes(4, reinterpret_cast<char*>(&submeshCount), &bytesRead))
        return result;

    result.reserve(submeshCount);

    for (uint32_t s = 0; s < submeshCount; ++s)
    {
        SubMeshData sub;

        uint64_t nameLen = 0;
        fh.ReadBytes(sizeof(nameLen), reinterpret_cast<char*>(&nameLen), &bytesRead);
        if (nameLen > 0)
        {
            sub.name.resize(nameLen);
            fh.ReadBytes(nameLen, sub.name.data(), &bytesRead);
        }

        fh.ReadBytes(16 * sizeof(float), reinterpret_cast<char*>(&sub.localTransform[0][0]), &bytesRead);

        uint64_t vCount = 0;
        fh.ReadBytes(sizeof(vCount), reinterpret_cast<char*>(&vCount), &bytesRead);
        sub.vertices.resize(vCount);
        fh.ReadBytes(vCount * sizeof(Vertex3D), reinterpret_cast<char*>(sub.vertices.data()), &bytesRead);

        uint64_t iCount = 0;
        fh.ReadBytes(sizeof(iCount), reinterpret_cast<char*>(&iCount), &bytesRead);
        sub.indices.resize(iCount);
        fh.ReadBytes(iCount * sizeof(uint32_t), reinterpret_cast<char*>(sub.indices.data()), &bytesRead);

        result.emplace_back(std::move(sub));
    }

    fh.Close();
    return result;
}

// ─── Importer interface ───────────────────────────────────────────────────────

bool ImporterMesh::Import(const MetaFileData& metaFileData)
{
    const aiScene* scene = aiImportFile(metaFileData.assetsPath.c_str(), ASSIMP_LOAD_FLAGS);

    if (!scene || !scene->HasMeshes())
    {
        NOUS_ERROR("Failed to load 3D Model: %s", metaFileData.assetsPath.c_str());
        return false;
    }

    std::vector<SubMeshData> submeshes;
    CollectSubMeshData(scene->mRootNode, scene, glm::mat4(1.0f), submeshes);
    aiReleaseImport(scene);

    if (submeshes.empty())
    {
        NOUS_ERROR("No submeshes found in: %s", metaFileData.assetsPath.c_str());
        return false;
    }

    return SaveSubmeshes(metaFileData, submeshes);
}

bool ImporterMesh::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    // Legacy path: convert a ResourceMesh into a single-submesh V2 binary.
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);

    SubMeshData sub;
    sub.name           = mesh->GetName();
    sub.localTransform = glm::mat4(1.0f);
    sub.vertices       = mesh->vertices;
    sub.indices.assign(mesh->indices.begin(), mesh->indices.end());

    const std::vector<SubMeshData> submeshes = { std::move(sub) };
    const bool ret = SaveSubmeshes(metaFileData, submeshes);
    NOUS_DELETE(mesh, MemoryTag::RESOURCE_MESH);
    return ret;
}

bool ImporterMesh::Deserialize(const std::string& libraryPath, Resource* outResource)
{
    // Merge all submeshes from the binary into a single ResourceMesh (CPU only).
    const auto submeshes = ParseMeshBinary(libraryPath);
    if (submeshes.empty()) return false;

    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);

    for (const auto& sub : submeshes)
    {
        const size_t prevV = mesh->vertices.size();
        mesh->vertices.insert(mesh->vertices.end(), sub.vertices.begin(), sub.vertices.end());

        const size_t prevI = mesh->indices.size();
        mesh->indices.resize(prevI + sub.indices.size());
        for (size_t i = 0; i < sub.indices.size(); ++i)
            mesh->indices[prevI + i] = sub.indices[i] + static_cast<uint32>(prevV);
    }

    // Compute local AABB once from the merged vertex set.
    if (!mesh->vertices.empty())
    {
        mesh->localAABBMin = mesh->vertices[0].position;
        mesh->localAABBMax = mesh->vertices[0].position;
        for (const auto& v : mesh->vertices)
        {
            mesh->localAABBMin = glm::min(mesh->localAABBMin, v.position);
            mesh->localAABBMax = glm::max(mesh->localAABBMax, v.position);
        }
    }

    return true;
}

bool ImporterMesh::Upload(Resource* outResource, IGPUResourceFactory* gpu)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);
    return gpu->CreateGeometry(
        mesh->vertices.size(), mesh->vertices.data(),
        mesh->indices.size(), mesh->indices.data(), mesh);
}

void ImporterMesh::Release(Resource* inResource, IGPUResourceFactory* gpu)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);
    if (mesh->internalID != INVALID_ID)
    {
        gpu->DestroyGeometry(mesh);
        mesh->internalID = INVALID_ID;
    }
}

void ImporterMesh::Evict(Resource* inResource)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);
    mesh->vertices.clear();
    mesh->indices.clear();
}

// ─── LoadHierarchy ────────────────────────────────────────────────────────────

std::vector<SubMeshData> ImporterMesh::LoadHierarchy(const std::string& libraryPath)
{
    return ParseMeshBinary(libraryPath);
}

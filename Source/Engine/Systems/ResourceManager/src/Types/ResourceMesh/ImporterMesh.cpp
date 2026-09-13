#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <EngineCore/InvalidID.h>
#include <EngineCore/Casts.h>
#include <FileSystem/FileHandle.h>

#include <ResourceManager/Import/ModelParser/ModelParser.h>
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <ResourceManager/Core/MetaFileData.h>

#include <MemoryManager/MemoryManager.h>

#include <Renderer/IGPUResourceFactory.h>

#include <Logger/Logger.h>

// No assimp here any more. This TU used to define ASSIMP_LOAD_FLAGS and include
// four assimp headers; all of that moved to ModelParser.cpp, which is now the
// engine's only assimp translation unit. The JSON, filesystem and hash-container
// includes went with the material fan-out that needed them.
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

// ImporterMesh itself is still a global-namespace type (the whole importer family
// is); the model pre-pass is namespaced like the rest of the new code. Pulling in
// just the two names keeps that boundary visible rather than dragging the whole
// namespace into a global-scope TU.
using nous::engine::resource_manager::ModelImportData;
using nous::engine::resource_manager::ParseModel;

// Reinterprets a trivially-copyable object or contiguous container as a
// writable byte span for FileHandle::ReadBytes.
template<typename T>
static std::span<char> AsWritableCharSpan(T& object)
{
    return std::span(reinterpret_cast<char*>(&object), sizeof(T));
}

template<typename T>
static std::span<char> AsWritableCharSpan(std::vector<T>& values)
{
    return std::span(reinterpret_cast<char*>(values.data()), values.size() * sizeof(T));
}

// ─── Binary format ────────────────────────────────────────────────────────────
//
//   magic:u32
//   submeshCount:u32
//   N × (blobOffset:u64, blobLength:u64)        <- directory, absolute offsets
//   N × blob
//
// blob:
//   nameLen:u64, name:chars
//   localTransform:16×float                     (column-major, GLM convention)
//   matPathLen:u64, matPath:chars               (empty => default material)
//   skeletonNameHash:u64                        (0 => model had no skeleton)
//   vertexCount:u64, Vertex3D[]
//   indexCount:u64, uint32_t[]
//
// THE DIRECTORY is why the format changed. Reading one submesh out of N used to
// mean parsing the whole file, and the spawn path did that N+1 times — once in
// SpawnMeshAsHierarchy, then once per submesh inside SubMeshCache, concurrently —
// so an O(N) result cost O(N²) bytes. Absolute blob offsets turn that into one
// small directory read plus one seek per submesh.
//
// Vertex3D also gained boneIDs/boneWeights (locations 7/8) in the same change.
//
// THERE IS NO BACKWARD COMPATIBILITY, deliberately. Library/ is a derived cache
// and Assets/ is the source of truth, so a binary written by an older engine is
// rejected with a message rather than parsed by a second code path. Bump the magic
// whenever the layout changes and delete Library/ to regenerate.
static constexpr uint32_t MESH_BINARY_MAGIC = 0xFA7C0DE4u;

// ─── Writing ──────────────────────────────────────────────────────────────────
//
// Blobs are serialized into memory first so their lengths — and therefore the
// directory — are known before anything reaches disk. FileHandle has no
// write-seek, and the vertex data is already in memory anyway.

static void AppendU32(std::vector<std::byte>& out, uint32_t v)
{
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(v));
}

static void AppendU64(std::vector<std::byte>& out, uint64_t v)
{
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(v));
}

static void AppendBytes(std::vector<std::byte>& out, const void* data, uint64_t size)
{
    const auto* p = static_cast<const std::byte*>(data);
    out.insert(out.end(), p, p + size);
}

static void AppendString(std::vector<std::byte>& out, const std::string& s)
{
    AppendU64(out, s.size());
    if (!s.empty()) AppendBytes(out, s.data(), s.size());
}

static void SerializeSubmesh(const SubMeshData& sub, std::vector<std::byte>& out)
{
    AppendString(out, sub.name);
    AppendBytes (out, &sub.localTransform[0][0], 16 * sizeof(float));
    AppendString(out, sub.materialAssetPath);
    AppendU64   (out, sub.skeletonNameHash);

    AppendU64(out, sub.vertices.size());
    if (!sub.vertices.empty())
        AppendBytes(out, sub.vertices.data(), sub.vertices.size() * sizeof(Vertex3D));

    AppendU64(out, sub.indices.size());
    if (!sub.indices.empty())
        AppendBytes(out, sub.indices.data(), sub.indices.size() * sizeof(uint32_t));
}

static bool SaveSubmeshes(const MetaFileData& metaFileData,
                          const std::vector<SubMeshData>& submeshes)
{
    const auto count = static_cast<uint32_t>(submeshes.size());

    // magic + count + N × (offset, length)
    const uint64_t headerSize =
        sizeof(uint32_t) * 2 + static_cast<uint64_t>(count) * (sizeof(uint64_t) * 2);

    std::vector<std::byte> blobs;
    std::vector<uint64_t>  offsets(count);
    std::vector<uint64_t>  lengths(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const uint64_t begin = blobs.size();
        SerializeSubmesh(submeshes[i], blobs);

        offsets[i] = headerSize + begin;
        lengths[i] = blobs.size() - begin;
    }

    std::vector<std::byte> file;
    file.reserve(headerSize + blobs.size());

    AppendU32(file, MESH_BINARY_MAGIC);
    AppendU32(file, count);

    for (uint32_t i = 0; i < count; ++i)
    {
        AppendU64(file, offsets[i]);
        AppendU64(file, lengths[i]);
    }

    file.insert(file.end(), blobs.begin(), blobs.end());

    FileHandle fh;
    if (!fh.Open(metaFileData.libraryPath, FileMode::WRITE, true))
        return false;

    const bool ok = fh.Write(std::span<const std::byte>(file)).has_value();
    fh.Close();

    return ok;
}

// ─── Reading ──────────────────────────────────────────────────────────────────

namespace
{
    struct BlobHeader
    {
        std::string name;
        glm::mat4   localTransform{ 1.0f };
        std::string materialAssetPath;
        uint64_t    skeletonNameHash = 0;
    };

    bool ReadU32(FileHandle& fh, uint32_t& out)
    {
        return fh.ReadBytes(AsWritableCharSpan(out)).has_value();
    }

    bool ReadU64(FileHandle& fh, uint64_t& out)
    {
        return fh.ReadBytes(AsWritableCharSpan(out)).has_value();
    }

    bool ReadString(FileHandle& fh, std::string& out)
    {
        uint64_t length = 0;
        if (!ReadU64(fh, length)) return false;

        out.clear();
        if (length == 0) return true;

        out.resize(length);
        return fh.ReadBytes(std::span(out.data(), length)).has_value();
    }

    // Reads the leading fields of a blob, leaving the cursor on the vertex count.
    bool ReadBlobHeader(FileHandle& fh, BlobHeader& out)
    {
        if (!ReadString(fh, out.name)) return false;

        if (!fh.ReadBytes(std::span(reinterpret_cast<char*>(&out.localTransform[0][0]),
                                    16 * sizeof(float))).has_value())
            return false;

        if (!ReadString(fh, out.materialAssetPath)) return false;

        return ReadU64(fh, out.skeletonNameHash);
    }

    bool ReadBlobGeometry(FileHandle& fh, SubMeshData& out)
    {
        uint64_t vCount = 0;
        if (!ReadU64(fh, vCount)) return false;

        out.vertices.resize(vCount);
        if (vCount > 0 && !fh.ReadBytes(AsWritableCharSpan(out.vertices)).has_value())
            return false;

        uint64_t iCount = 0;
        if (!ReadU64(fh, iCount)) return false;

        out.indices.resize(iCount);
        if (iCount > 0 && !fh.ReadBytes(AsWritableCharSpan(out.indices)).has_value())
            return false;

        return true;
    }

    // Opens the file and consumes magic + count. A wrong magic is an outdated or
    // corrupt binary; both mean "reimport", so both get the same message.
    bool OpenAndReadHeader(const std::string& libraryPath, const char* caller,
                           FileHandle& fh, uint32_t& outCount)
    {
        if (!fh.Open(libraryPath, FileMode::READ, true))
        {
            NOUS_ERROR("%s: failed to open '%s'", caller, libraryPath.c_str());
            return false;
        }

        uint32_t magic = 0;
        if (!ReadU32(fh, magic)) return false;

        if (magic != MESH_BINARY_MAGIC)
        {
            NOUS_ERROR("%s: '%s' is not a current mesh binary (magic 0x%08X, expected 0x%08X). "
                       "Delete Library/ and reimport the asset.",
                       caller, libraryPath.c_str(), magic, MESH_BINARY_MAGIC);
            return false;
        }

        return ReadU32(fh, outCount);
    }

    // Directory entries are (absolute offset, length).
    bool ReadDirectory(FileHandle& fh, uint32_t count,
                       std::vector<std::pair<uint64_t, uint64_t>>& out)
    {
        out.resize(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            if (!ReadU64(fh, out[i].first) || !ReadU64(fh, out[i].second)) return false;
        }

        return true;
    }
}

// Full parse: every submesh, geometry included. Only Deserialize wants this — it
// merges them all into one ResourceMesh.
static std::vector<SubMeshData> ParseMeshBinary(const std::string& libraryPath)
{
    std::vector<SubMeshData> result;

    FileHandle fh;
    uint32_t   submeshCount = 0;

    if (!OpenAndReadHeader(libraryPath, "ParseMeshBinary", fh, submeshCount)) return result;

    // The directory is read past, not used: a full parse walks every blob in order
    // anyway, so seeking per blob would only add syscalls.
    std::vector<std::pair<uint64_t, uint64_t>> directory;
    if (!ReadDirectory(fh, submeshCount, directory)) return result;

    result.reserve(submeshCount);

    for (uint32_t s = 0; s < submeshCount; ++s)
    {
        BlobHeader header;
        if (!ReadBlobHeader(fh, header)) break;

        SubMeshData sub;
        sub.name              = std::move(header.name);
        sub.localTransform    = header.localTransform;
        sub.materialAssetPath = std::move(header.materialAssetPath);
        sub.skeletonNameHash  = header.skeletonNameHash;

        if (!ReadBlobGeometry(fh, sub)) break;

        result.emplace_back(std::move(sub));
    }

    fh.Close();
    return result;
}

// ─── Importer interface ───────────────────────────────────────────────────────

bool ImporterMesh::SaveModel(const MetaFileData& metaFileData, const ModelImportData& model)
{
    if (model.submeshes.empty())
    {
        NOUS_ERROR("No submeshes found in: %s", metaFileData.assetsPath.c_str());
        return false;
    }

    return SaveSubmeshes(metaFileData, model.submeshes);
}

// ─── LoadHierarchy ────────────────────────────────────────────────────────────

std::vector<SubMeshData> ImporterMesh::LoadHierarchy(const std::string& libraryPath)
{
    return ParseMeshBinary(libraryPath);
}

// ─── Targeted readers ─────────────────────────────────────────────────────────
// The point of the directory: reach one submesh without touching the others.

std::vector<SubMeshInfo> ImporterMesh::LoadSubmeshInfo(const std::string& libraryPath)
{
    std::vector<SubMeshInfo> result;

    FileHandle fh;
    uint32_t   submeshCount = 0;

    if (!OpenAndReadHeader(libraryPath, "LoadSubmeshInfo", fh, submeshCount)) return result;

    std::vector<std::pair<uint64_t, uint64_t>> directory;
    if (!ReadDirectory(fh, submeshCount, directory)) return result;

    result.reserve(submeshCount);

    for (uint32_t s = 0; s < submeshCount; ++s)
    {
        // Seek to the blob and read only its header. The vertex and index arrays
        // that follow are never touched.
        if (!fh.Seek(directory[s].first)) break;

        BlobHeader header;
        if (!ReadBlobHeader(fh, header)) break;

        result.push_back({ std::move(header.name), header.localTransform,
                           std::move(header.materialAssetPath) });
    }

    fh.Close();
    return result;
}

bool ImporterMesh::LoadSubmesh(const std::string& libraryPath, int32_t submeshIndex,
                               SubMeshData& out)
{
    if (submeshIndex < 0) return false;

    FileHandle fh;
    uint32_t   submeshCount = 0;

    if (!OpenAndReadHeader(libraryPath, "LoadSubmesh", fh, submeshCount)) return false;

    if (static_cast<uint32_t>(submeshIndex) >= submeshCount)
    {
        NOUS_ERROR("LoadSubmesh: index %d out of range (count=%u) for '%s'",
                   submeshIndex, submeshCount, libraryPath.c_str());
        return false;
    }

    std::vector<std::pair<uint64_t, uint64_t>> directory;
    if (!ReadDirectory(fh, submeshCount, directory)) return false;

    if (!fh.Seek(directory[static_cast<size_t>(submeshIndex)].first)) return false;

    BlobHeader header;
    if (!ReadBlobHeader(fh, header)) return false;

    out.name              = std::move(header.name);
    out.localTransform    = header.localTransform;
    out.materialAssetPath = std::move(header.materialAssetPath);
    out.skeletonNameHash  = header.skeletonNameHash;

    const bool ok = ReadBlobGeometry(fh, out);

    fh.Close();
    return ok;
}

bool ImporterMesh::Import(const MetaFileData& metaFileData)
{
    // This importer no longer parses anything. The whole Assimp side -- scene
    // traversal, the material/texture fan-out, and now skeleton and clip
    // extraction -- lives in ModelParser, the engine's only assimp translation
    // unit. What is left here is serialization, which is what a mesh importer
    // should have been doing all along.
    //
    // This is now the FALLBACK path, and it DISCARDS model.skeleton and model.clips.
    // The normal route is ImportPipeline::Dispatch -> ImportModel, which parses once
    // and hands each importer its slice (SaveModel above, plus SaveSkeleton and
    // SaveClip). Anything reaching this function either is not a model file at all
    // (a .nmesh written back out from procedural geometry) or bypassed Dispatch.
    auto model = ParseModel(metaFileData.assetsPath, m_resources);

    if (!model)
    {
        NOUS_ERROR("Failed to load 3D Model: %s", model.error().c_str());
        return false;
    }

    return SaveModel(metaFileData, *model);
}

bool ImporterMesh::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    // Writes a runtime-built ResourceMesh back out as a one-submesh binary. Used
    // for meshes that never came from a model file (procedural geometry), which is
    // why the transform is identity and there is no material reference.
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

bool ImporterMesh::Deserialize(const std::string& libraryPath, ResourceBase* outResource)
{
    // Merge all submeshes from the binary into a single ResourceMesh (CPU only).
    const auto submeshes = ParseMeshBinary(libraryPath);
    if (submeshes.empty()) return false;

    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);
    mesh->vertices.clear();
    mesh->indices.clear();

    for (const auto& sub : submeshes)
    {
        const size_t prevV = mesh->vertices.size();
        mesh->vertices.insert(mesh->vertices.end(), sub.vertices.begin(), sub.vertices.end());

        const size_t prevI = mesh->indices.size();
        mesh->indices.resize(prevI + sub.indices.size());
        for (size_t i = 0; i < sub.indices.size(); ++i)
            mesh->indices[prevI + i] = sub.indices[i] + static_cast<uint32_t>(prevV);
    }

    // Every submesh here came from ONE model, so they share a rig; taking the first
    // is exact rather than a guess. NOT derived from the vertices, so it cannot live
    // in RecomputeDerivedData.
    mesh->skeletonNameHash = submeshes.empty() ? 0 : submeshes.front().skeletonNameHash;

    // Local AABB + hasSkinning, in one pass over the merged vertex set.
    mesh->RecomputeDerivedData();

    return true;
}

bool ImporterMesh::Upload(ResourceBase* outResource, IGPUResourceFactory* gpu)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);
    return gpu->CreateGeometry(
        mesh->vertices.size(), mesh->vertices.data(),
        mesh->indices.size(), mesh->indices.data(), mesh);
}

void ImporterMesh::Release(ResourceBase* inResource, IGPUResourceFactory* gpu)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);
    if (mesh->internalID != INVALID_ID)
    {
        gpu->DestroyGeometry(mesh);
        mesh->internalID = INVALID_ID;
    }
}

void ImporterMesh::Evict(ResourceBase* inResource)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);
    mesh->vertices.clear();
    mesh->indices.clear();
}

#include <ResourceManager/Types/ResourceSkeleton/ImporterSkeleton.h>

#include <EngineCore/Casts.h>
#include <FileSystem/FileHandle.h>
#include <Logger/Logger.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Import/ModelParser/ModelParser.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <cstddef>
#include <span>
#include <vector>

using nous::engine::animation_system::SkeletonData;
using nous::engine::animation_system::Transform;

// ─── Binary format ────────────────────────────────────────────────────────────
//
//   magic:u32 = 'NSKL'
//   boneCount:u32
//   nameHash:u64
//   per bone:
//     nameLen:u64, name:chars
//     parent:i32                          (-1 root; parent < index guaranteed)
//     offset:16 x f32                     (glm::mat4, column-major)
//     bindLocal: 3 x f32 pos, 4 x f32 rot (w,x,y,z), 3 x f32 scale
//
// String lengths are u64 to match ImporterMesh.cpp's AppendString/ReadString. A
// second string-length convention in a sibling format buys nothing and costs an
// afternoon to a reader who assumes they agree.
//
// ONE READ PATH. Any other magic is rejected outright rather than parsed:
// Library/ is a derived cache and Assets/ is the source of truth, so regeneration
// IS the migration and a second read path would just be a second thing to keep
// correct. Bump the magic when the layout changes.
static constexpr uint32_t SKELETON_BINARY_MAGIC = 0x4E534B4Cu;

namespace
{
    void AppendU32(std::vector<std::byte>& out, uint32_t v)
    {
        const auto* p = reinterpret_cast<const std::byte*>(&v);
        out.insert(out.end(), p, p + sizeof(v));
    }

    void AppendU64(std::vector<std::byte>& out, uint64_t v)
    {
        const auto* p = reinterpret_cast<const std::byte*>(&v);
        out.insert(out.end(), p, p + sizeof(v));
    }

    void AppendI32(std::vector<std::byte>& out, int32_t v)
    {
        const auto* p = reinterpret_cast<const std::byte*>(&v);
        out.insert(out.end(), p, p + sizeof(v));
    }

    void AppendBytes(std::vector<std::byte>& out, const void* data, uint64_t size)
    {
        const auto* p = static_cast<const std::byte*>(data);
        out.insert(out.end(), p, p + size);
    }

    void AppendString(std::vector<std::byte>& out, const std::string& s)
    {
        AppendU64(out, s.size());
        if (!s.empty()) AppendBytes(out, s.data(), s.size());
    }

    template<typename T>
    std::span<char> AsWritableCharSpan(T& object)
    {
        return std::span(reinterpret_cast<char*>(&object), sizeof(T));
    }

    bool ReadU32(FileHandle& fh, uint32_t& out) { return fh.ReadBytes(AsWritableCharSpan(out)).has_value(); }
    bool ReadU64(FileHandle& fh, uint64_t& out) { return fh.ReadBytes(AsWritableCharSpan(out)).has_value(); }
    bool ReadI32(FileHandle& fh, int32_t&  out) { return fh.ReadBytes(AsWritableCharSpan(out)).has_value(); }

    bool ReadString(FileHandle& fh, std::string& out)
    {
        uint64_t length = 0;
        if (!ReadU64(fh, length)) return false;

        out.assign(length, '\0');
        if (length == 0) return true;

        return fh.ReadBytes(std::span(out.data(), length)).has_value();
    }

    bool ReadFloats(FileHandle& fh, float* out, size_t count)
    {
        return fh.ReadBytes(std::span(reinterpret_cast<char*>(out), count * sizeof(float))).has_value();
    }

    // A quaternion is written and read COMPONENT-WISE, never as raw bytes.
    //
    // glm::quat's default memory layout is {x, y, z, w} -- w is the LAST member, not
    // the first. So `&quat.w` is not the start of the object, and dumping
    // 4 * sizeof(float) from it reads twelve bytes PAST the end of the quaternion.
    // In Transform that lands in the adjacent `scale`, which makes the bug nearly
    // invisible: scale round-trips fine (written and restored by the same overrun)
    // while the rotation silently loses x, y and z.
    //
    // The file format stores w,x,y,z -- matching the JSON convention used for
    // CTransform elsewhere in the engine -- and these two functions are the only
    // place that ordering is expressed.
    void AppendQuat(std::vector<std::byte>& out, const glm::quat& q)
    {
        const float wxyz[4] = { q.w, q.x, q.y, q.z };
        AppendBytes(out, wxyz, sizeof(wxyz));
    }

    bool ReadQuat(FileHandle& fh, glm::quat& out)
    {
        float wxyz[4] = {};
        if (!ReadFloats(fh, wxyz, 4)) return false;

        out = glm::quat(wxyz[0], wxyz[1], wxyz[2], wxyz[3]);   // (w, x, y, z) ctor
        return true;
    }
}

namespace nous::engine::resource_manager
{
    uint64_t HashBoneNames(const std::vector<std::string>& names)
    {
        constexpr uint64_t c_offsetBasis = 14695981039346656037ull;
        constexpr uint64_t c_prime       = 1099511628211ull;

        uint64_t hash = c_offsetBasis;

        const auto feed = [&hash](const char c)
        {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= c_prime;
        };

        // The '\n' after every name (not between them) is what stops {"ab","c"} and
        // {"a","bc"} hashing alike.
        for (const std::string& name : names)
        {
            for (const char c : name) feed(c);
            feed('\n');
        }

        return hash;
    }
}

bool ImporterSkeleton::SaveSkeleton(const MetaFileData& metaFileData, const SkeletonData& skeleton)
{
    if (!skeleton.IsConsistent())
    {
        NOUS_ERROR("ImporterSkeleton: refusing to write '%s' — array lengths disagree.",
                   metaFileData.libraryPath.c_str());
        return false;
    }

    const auto boneCount = static_cast<uint32_t>(skeleton.BoneCount());

    std::vector<std::byte> file;
    AppendU32(file, SKELETON_BINARY_MAGIC);
    AppendU32(file, boneCount);
    AppendU64(file, nous::engine::resource_manager::HashBoneNames(skeleton.names));

    for (uint32_t b = 0; b < boneCount; ++b)
    {
        AppendString(file, skeleton.names[b]);
        AppendI32   (file, skeleton.parents[b]);
        AppendBytes (file, &skeleton.offsets[b][0][0], 16 * sizeof(float));

        // vec3 really is {x,y,z} contiguous, so those two are safe as raw bytes.
        // The quaternion is NOT -- see AppendQuat.
        const Transform& local = skeleton.bindLocals[b];
        AppendBytes(file, &local.position.x, 3 * sizeof(float));
        AppendQuat (file, local.rotation);
        AppendBytes(file, &local.scale.x,    3 * sizeof(float));
    }

    FileHandle fh;
    if (!fh.Open(metaFileData.libraryPath, FileMode::WRITE, true))
    {
        NOUS_ERROR("ImporterSkeleton: could not open '%s' for writing.",
                   metaFileData.libraryPath.c_str());
        return false;
    }

    const bool ok = fh.Write(std::span<const std::byte>(file)).has_value();
    fh.Close();

    return ok;
}

bool ImporterSkeleton::Deserialize(const std::string& libraryPath, ResourceBase* resource)
{
    auto* target = down_cast<ResourceSkeleton*>(resource);

    // Cleared up front so a rejected file leaves an empty skeleton rather than
    // whatever half-read state the failure happened to stop at.
    target->skeleton = SkeletonData{};
    target->nameHash = 0;

    FileHandle fh;
    if (!fh.Open(libraryPath, FileMode::READ, true)) return false;

    uint32_t magic = 0;
    if (!ReadU32(fh, magic)) { fh.Close(); return false; }

    if (magic != SKELETON_BINARY_MAGIC)
    {
        NOUS_ERROR("ImporterSkeleton: '%s' has magic 0x%X, expected 0x%X. "
                   "Delete Library/ and reimport — this format has one read path.",
                   libraryPath.c_str(), magic, SKELETON_BINARY_MAGIC);
        fh.Close();
        return false;
    }

    uint32_t boneCount = 0;
    uint64_t nameHash  = 0;
    if (!ReadU32(fh, boneCount) || !ReadU64(fh, nameHash)) { fh.Close(); return false; }

    SkeletonData skeleton;
    skeleton.names.resize(boneCount);
    skeleton.parents.resize(boneCount);
    skeleton.offsets.resize(boneCount, glm::mat4(1.0f));
    skeleton.bindLocals.resize(boneCount);

    bool ok = true;
    for (uint32_t b = 0; b < boneCount && ok; ++b)
    {
        Transform& local = skeleton.bindLocals[b];

        ok = ReadString(fh, skeleton.names[b])
          && ReadI32   (fh, skeleton.parents[b])
          && ReadFloats(fh, &skeleton.offsets[b][0][0], 16)
          && ReadFloats(fh, &local.position.x, 3)
          && ReadQuat  (fh, local.rotation)
          && ReadFloats(fh, &local.scale.x,    3);
    }

    fh.Close();

    if (!ok)
    {
        NOUS_ERROR("ImporterSkeleton: '%s' ended early — the file is truncated.",
                   libraryPath.c_str());
        return false;
    }

    // Never serialized: a string map in a binary is dead weight beside the names
    // array it duplicates. Skip this and every FindBone silently returns -1.
    skeleton.RebuildLookup();

    target->skeleton = std::move(skeleton);
    target->nameHash = nameHash;

    return true;
}

bool ImporterSkeleton::Import(const MetaFileData& metaFileData)
{
    // Fallback: the stub outlived its library binary, so re-parse the source model.
    const JsonObject stub = JsonFile::LoadFromFile(metaFileData.assetsPath);
    const std::string source = stub.GetString("source");

    if (source.empty())
    {
        NOUS_ERROR("ImporterSkeleton: '%s' names no source model.",
                   metaFileData.assetsPath.c_str());
        return false;
    }

    auto model = nous::engine::resource_manager::ParseModel(source, m_resources);
    if (!model)
    {
        NOUS_ERROR("ImporterSkeleton: %s", model.error().c_str());
        return false;
    }

    if (!model->HasSkeleton())
    {
        NOUS_ERROR("ImporterSkeleton: '%s' no longer contains a skeleton.", source.c_str());
        return false;
    }

    return SaveSkeleton(metaFileData, model->skeleton);
}

bool ImporterSkeleton::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    return SaveSkeleton(metaFileData, down_cast<ResourceSkeleton*>(inResource)->skeleton);
}

void ImporterSkeleton::Evict(ResourceBase* resource)
{
    auto* target = down_cast<ResourceSkeleton*>(resource);
    target->skeleton = SkeletonData{};
    target->nameHash = 0;
}

bool ImporterSkeleton::Upload(ResourceBase*, IGPUResourceFactory*) { return true; }

void ImporterSkeleton::Release(ResourceBase*, IGPUResourceFactory*) {}

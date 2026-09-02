#include <ResourceManager/Types/ResourceAnimation/ImporterAnimation.h>

#include <EngineCore/Casts.h>
#include <FileSystem/FileHandle.h>
#include <Logger/Logger.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Import/ModelParser/ModelParser.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <cstddef>
#include <span>
#include <vector>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::AnimClipData;

// ─── Binary format ────────────────────────────────────────────────────────────
//
//   magic:u32 = 'NANM'
//   nameLen:u64, name:chars
//   duration:f32                          (SECONDS -- ticks died at BuildClip)
//   channelCount:u32
//   per channel:
//     nameLen:u64, boneName:chars
//     posCount:u32,   posTimes[]:f32,   posValues[]:3 x f32
//     rotCount:u32,   rotTimes[]:f32,   rotValues[]:4 x f32 (w,x,y,z)
//     scaleCount:u32, scaleTimes[]:f32, scaleValues[]:3 x f32
//
// Each track's TIMES are written as one contiguous run BEFORE its values, which
// preserves on disk the split-array layout the per-channel cursor scan depends on:
// the bracketing-key search walks a dense run of floats instead of striding over
// key structs whose values it will not read. A key-struct format would have to be
// transposed on every load.
//
// The three tracks are counted independently because they ARE independent -- an
// exporter routinely writes position keys and no scale keys.
//
// ONE READ PATH. Any other magic is rejected outright rather than parsed; Library/
// is a derived cache, so regeneration IS the migration.
static constexpr uint32_t ANIMATION_BINARY_MAGIC = 0x4E414E4Du;

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

    void AppendF32(std::vector<std::byte>& out, float v)
    {
        AppendBytes(out, &v, sizeof(v));
    }

    // glm::vec3 really is {x,y,z} contiguous, so a vec3 track is a straight dump.
    void AppendTrack(std::vector<std::byte>& out,
                     const std::vector<float>& times, const std::vector<glm::vec3>& values)
    {
        const auto count = static_cast<uint32_t>(times.size());
        AppendU32(out, count);
        if (count == 0) return;

        AppendBytes(out, times.data(),  count * sizeof(float));
        AppendBytes(out, values.data(), count * sizeof(glm::vec3));
    }

    // The rotation track is NOT a straight dump. glm::quat's memory layout is
    // {x,y,z,w} -- w is the LAST member -- so dumping the vector's bytes would write
    // x,y,z,w while the format (and every other quat in this engine, JSON included)
    // says w,x,y,z. Converting per key is the only place that ordering is expressed.
    void AppendTrack(std::vector<std::byte>& out,
                     const std::vector<float>& times, const std::vector<glm::quat>& values)
    {
        const auto count = static_cast<uint32_t>(times.size());
        AppendU32(out, count);
        if (count == 0) return;

        AppendBytes(out, times.data(), count * sizeof(float));

        for (const glm::quat& q : values)
        {
            const float wxyz[4] = { q.w, q.x, q.y, q.z };
            AppendBytes(out, wxyz, sizeof(wxyz));
        }
    }

    template<typename T>
    std::span<char> AsWritableCharSpan(T& object)
    {
        return std::span(reinterpret_cast<char*>(&object), sizeof(T));
    }

    bool ReadU32(FileHandle& fh, uint32_t& out) { return fh.ReadBytes(AsWritableCharSpan(out)).has_value(); }
    bool ReadU64(FileHandle& fh, uint64_t& out) { return fh.ReadBytes(AsWritableCharSpan(out)).has_value(); }
    bool ReadF32(FileHandle& fh, float&    out) { return fh.ReadBytes(AsWritableCharSpan(out)).has_value(); }

    bool ReadString(FileHandle& fh, std::string& out)
    {
        uint64_t length = 0;
        if (!ReadU64(fh, length)) return false;

        out.assign(length, '\0');
        if (length == 0) return true;

        return fh.ReadBytes(std::span(out.data(), length)).has_value();
    }

    bool ReadTimes(FileHandle& fh, std::vector<float>& times, uint32_t count)
    {
        times.assign(count, 0.0f);
        if (count == 0) return true;

        return fh.ReadBytes(std::span(reinterpret_cast<char*>(times.data()),
                                      count * sizeof(float))).has_value();
    }

    bool ReadTrack(FileHandle& fh, std::vector<float>& times, std::vector<glm::vec3>& values)
    {
        uint32_t count = 0;
        if (!ReadU32(fh, count)) return false;
        if (!ReadTimes(fh, times, count)) return false;

        values.assign(count, glm::vec3(0.0f));
        if (count == 0) return true;

        return fh.ReadBytes(std::span(reinterpret_cast<char*>(values.data()),
                                      count * sizeof(glm::vec3))).has_value();
    }

    bool ReadTrack(FileHandle& fh, std::vector<float>& times, std::vector<glm::quat>& values)
    {
        uint32_t count = 0;
        if (!ReadU32(fh, count)) return false;
        if (!ReadTimes(fh, times, count)) return false;

        values.assign(count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

        for (uint32_t i = 0; i < count; ++i)
        {
            float wxyz[4] = {};
            if (!fh.ReadBytes(std::span(reinterpret_cast<char*>(wxyz), sizeof(wxyz))).has_value())
                return false;

            values[i] = glm::quat(wxyz[0], wxyz[1], wxyz[2], wxyz[3]);   // (w, x, y, z) ctor
        }

        return true;
    }
}

bool ImporterAnimation::SaveClip(const MetaFileData& metaFileData, const AnimClipData& clip)
{
    std::vector<std::byte> file;

    AppendU32   (file, ANIMATION_BINARY_MAGIC);
    AppendString(file, clip.name);
    AppendF32   (file, clip.duration);
    AppendU32   (file, static_cast<uint32_t>(clip.channels.size()));

    for (const AnimChannel& channel : clip.channels)
    {
        AppendString(file, channel.boneName);
        AppendTrack (file, channel.posTimes,   channel.posValues);
        AppendTrack (file, channel.rotTimes,   channel.rotValues);
        AppendTrack (file, channel.scaleTimes, channel.scaleValues);
    }

    FileHandle fh;
    if (!fh.Open(metaFileData.libraryPath, FileMode::WRITE, true))
    {
        NOUS_ERROR("ImporterAnimation: could not open '%s' for writing.",
                   metaFileData.libraryPath.c_str());
        return false;
    }

    const bool ok = fh.Write(std::span<const std::byte>(file)).has_value();
    fh.Close();

    return ok;
}

bool ImporterAnimation::Deserialize(const std::string& libraryPath, ResourceBase* resource)
{
    auto* target = down_cast<ResourceAnimation*>(resource);

    // Cleared up front so a rejected file leaves an empty clip rather than whatever
    // half-read state the failure happened to stop at.
    target->clip = AnimClipData{};

    FileHandle fh;
    if (!fh.Open(libraryPath, FileMode::READ, true)) return false;

    uint32_t magic = 0;
    if (!ReadU32(fh, magic)) { fh.Close(); return false; }

    if (magic != ANIMATION_BINARY_MAGIC)
    {
        NOUS_ERROR("ImporterAnimation: '%s' has magic 0x%X, expected 0x%X. "
                   "Delete Library/ and reimport — this format has one read path.",
                   libraryPath.c_str(), magic, ANIMATION_BINARY_MAGIC);
        fh.Close();
        return false;
    }

    AnimClipData clip;
    uint32_t channelCount = 0;

    bool ok = ReadString(fh, clip.name)
           && ReadF32   (fh, clip.duration)
           && ReadU32   (fh, channelCount);

    if (ok)
    {
        clip.channels.resize(channelCount);

        for (uint32_t c = 0; c < channelCount && ok; ++c)
        {
            AnimChannel& channel = clip.channels[c];

            ok = ReadString(fh, channel.boneName)
              && ReadTrack (fh, channel.posTimes,   channel.posValues)
              && ReadTrack (fh, channel.rotTimes,   channel.rotValues)
              && ReadTrack (fh, channel.scaleTimes, channel.scaleValues);
        }
    }

    fh.Close();

    if (!ok)
    {
        NOUS_ERROR("ImporterAnimation: '%s' ended early — the file is truncated.",
                   libraryPath.c_str());
        return false;
    }

    target->clip = std::move(clip);

    return true;
}

bool ImporterAnimation::Import(const MetaFileData& metaFileData)
{
    // Fallback: the stub outlived its library binary, so re-parse the source model.
    const JsonObject stub = JsonFile::LoadFromFile(metaFileData.assetsPath);
    const std::string source   = stub.GetString("source");
    const std::string clipName = stub.GetString("clip");

    if (source.empty())
    {
        NOUS_ERROR("ImporterAnimation: '%s' names no source model.",
                   metaFileData.assetsPath.c_str());
        return false;
    }

    auto model = nous::engine::resource_manager::ParseModel(source, m_resources);
    if (!model)
    {
        NOUS_ERROR("ImporterAnimation: %s", model.error().c_str());
        return false;
    }

    for (const AnimClipData& clip : model->clips)
    {
        if (clip.name == clipName)
            return SaveClip(metaFileData, clip);
    }

    NOUS_ERROR("ImporterAnimation: '%s' no longer contains a clip named '%s'.",
               source.c_str(), clipName.c_str());
    return false;
}

bool ImporterAnimation::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    return SaveClip(metaFileData, down_cast<ResourceAnimation*>(inResource)->clip);
}

void ImporterAnimation::Evict(ResourceBase* resource)
{
    down_cast<ResourceAnimation*>(resource)->clip = AnimClipData{};
}

bool ImporterAnimation::Upload(ResourceBase*, IGPUResourceFactory*) { return true; }

void ImporterAnimation::Release(ResourceBase*, IGPUResourceFactory*) {}

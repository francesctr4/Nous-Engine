#include <ResourceManager/Types/ResourceAnimation/ImporterAnimation.h>

#include <EngineCore/Casts.h>
#include <FileSystem/FileHandle.h>
#include <Logger/Logger.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Import/ModelParser/ModelParser.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <AnimationSystem/Events/AnimationEvents.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <cstddef>
#include <span>
#include <vector>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::AnimClipData;
using nous::engine::animation_system::AnimationEvent;

// ─── Binary format ────────────────────────────────────────────────────────────
//
//   magic:u32 = 'NAN3'
//   nameLen:u64, name:chars
//   duration:f32                          (SECONDS -- ticks died at BuildClip)
//   loop:u32                              (0/1 -- per-clip settings, see below)
//   speed:f32
//   eventCount:u32
//   per event:
//     time:f32
//     nameLen:u64,        name:chars
//     floatParam:f32
//     stringParamLen:u64, stringParam:chars
//   channelCount:u32
//   per channel:
//     nameLen:u64, boneName:chars
//     posCount:u32,   posTimes[]:f32,   posValues[]:3 x f32
//     rotCount:u32,   rotTimes[]:f32,   rotValues[]:4 x f32 (w,x,y,z)
//     scaleCount:u32, scaleTimes[]:f32, scaleValues[]:3 x f32
//
// Each track's TIMES are one contiguous run BEFORE its values, preserving on disk the
// split-array layout the per-channel cursor scan depends on. The three tracks are counted
// independently because they ARE independent -- an exporter routinely writes position keys
// and no scale keys.
//
// loop/speed/events are AUTHORING data and also live in the stub, but the binary carries
// its own copy because an exported game ships Library/ and no Assets/. They sit before
// channelCount so a reader reaches them without walking the channels.
//
// ONE READ PATH: any other magic is rejected rather than parsed. Library/ is a derived
// cache, so regeneration IS the migration -- delete it and reimport.
static constexpr uint32_t ANIMATION_BINARY_MAGIC = 0x4E414E33u;   // 'NAN3'

// The stub's authoring keys. Absent means default -- EnsureStub writes none of them.
static constexpr const char* STUB_KEY_LOOP   = "loop";
static constexpr const char* STUB_KEY_SPEED  = "speed";
static constexpr const char* STUB_KEY_EVENTS = "events";

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

bool ImporterAnimation::SaveClip(const MetaFileData& metaFileData, const AnimClipData& clip,
                                 const ClipAuthoring& authoring)
{
    // Sorted by the WRITER so the runtime's emission order is time order regardless
    // of how the timeline window or a hand-edited stub happened to order them. The
    // collector does not require it, which is why this is the only place it happens.
    std::vector<AnimationEvent> events = authoring.events;
    nous::engine::animation_system::SortEvents(events);

    std::vector<std::byte> file;

    AppendU32   (file, ANIMATION_BINARY_MAGIC);
    AppendString(file, clip.name);
    AppendF32   (file, clip.duration);
    AppendU32   (file, authoring.settings.loop ? 1u : 0u);
    AppendF32   (file, authoring.settings.speed);

    AppendU32(file, static_cast<uint32_t>(events.size()));
    for (const AnimationEvent& event : events)
    {
        AppendF32   (file, event.time);
        AppendString(file, event.name);
        AppendF32   (file, event.floatParam);
        AppendString(file, event.stringParam);
    }

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
    target->clip     = AnimClipData{};
    target->settings = AnimationSettings{};
    target->events.clear();

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
    AnimationSettings settings;
    std::vector<AnimationEvent> events;
    uint32_t loopFlag     = 1;
    uint32_t eventCount   = 0;
    uint32_t channelCount = 0;

    bool ok = ReadString(fh, clip.name)
           && ReadF32   (fh, clip.duration)
           && ReadU32   (fh, loopFlag)
           && ReadF32   (fh, settings.speed)
           && ReadU32   (fh, eventCount);

    settings.loop = loopFlag != 0;

    if (ok)
    {
        events.resize(eventCount);

        for (uint32_t e = 0; e < eventCount && ok; ++e)
        {
            AnimationEvent& event = events[e];

            ok = ReadF32   (fh, event.time)
              && ReadString(fh, event.name)
              && ReadF32   (fh, event.floatParam)
              && ReadString(fh, event.stringParam);
        }
    }

    ok = ok && ReadU32(fh, channelCount);

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

    target->clip     = std::move(clip);
    target->settings = settings;
    target->events   = std::move(events);

    return true;
}

ClipAuthoring ImporterAnimation::ReadAuthoringFromStub(const std::string& stubPath)
{
    const JsonObject stub = JsonFile::LoadFromFile(stubPath);

    // Each Get* falls back to the struct's own default, so an unreadable stub and a
    // stub declaring no keys produce the same thing -- which is what makes an
    // unedited stub (the EnsureStub output) the common case rather than an error.
    ClipAuthoring authoring;
    authoring.settings.loop  = stub.GetBool (STUB_KEY_LOOP,  authoring.settings.loop);
    authoring.settings.speed = stub.GetFloat(STUB_KEY_SPEED, authoring.settings.speed);

    const JsonArray events = stub.GetArray(STUB_KEY_EVENTS);
    for (int i = 0; i < events.Count(); ++i)
    {
        const JsonObject entry = events.GetObject(i);

        AnimationEvent event;
        event.time        = entry.GetFloat ("time");
        event.name        = entry.GetString("name");
        event.floatParam  = entry.GetFloat ("float");
        event.stringParam = entry.GetString("string");

        // An unnamed event can match nothing a script tests for, so it is authoring
        // noise rather than data -- dropping it keeps the runtime list meaningful.
        if (!event.name.empty())
            authoring.events.push_back(std::move(event));
    }

    nous::engine::animation_system::SortEvents(authoring.events);
    return authoring;
}

bool ImporterAnimation::WriteAuthoringToStub(const std::string& stubPath,
                                             const ClipAuthoring& authoring)
{
    // Read-modify-write: "source" and "clip" are what the fallback Import() re-parse
    // needs, and writing a fresh object would drop them.
    JsonObject stub = JsonFile::LoadFromFile(stubPath);
    stub.Set(STUB_KEY_LOOP,  authoring.settings.loop);
    stub.Set(STUB_KEY_SPEED, authoring.settings.speed);

    std::vector<AnimationEvent> events = authoring.events;
    nous::engine::animation_system::SortEvents(events);

    JsonArray array;
    for (const AnimationEvent& event : events)
    {
        JsonObject entry;
        entry.Set("time",   event.time);
        entry.Set("name",   event.name);
        entry.Set("float",  event.floatParam);
        entry.Set("string", event.stringParam);
        array.Append(std::move(entry));
    }
    stub.Set(STUB_KEY_EVENTS, std::move(array));

    if (!JsonFile::SaveToFile(stub, stubPath))
    {
        NOUS_ERROR("ImporterAnimation: could not write authoring data to stub '%s'.",
                   stubPath.c_str());
        return false;
    }

    return true;
}

bool ImporterAnimation::SaveAuthoring(ResourceAnimation& animation)
{
    ClipAuthoring authoring;
    authoring.settings = animation.settings;
    authoring.events   = animation.events;

    const bool stubOk = WriteAuthoringToStub(animation.GetAssetsPath(), authoring);

    MetaFileData meta;
    meta.uid          = animation.GetUID();
    meta.name         = animation.GetName();
    meta.resourceType = ResourceType::ANIMATION;
    meta.assetsPath   = animation.GetAssetsPath();
    meta.libraryPath  = animation.GetLibraryPath();

    // Deliberately sequenced into locals rather than `a && b`: both writes must be
    // attempted even when the first fails, or a read-only stub would also leave the
    // binary stale.
    const bool binaryOk = SaveClip(meta, animation.clip, authoring);

    return stubOk && binaryOk;
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

    // The stub is the only surviving record of the authored data on this path -- it is
    // precisely the library binary that went missing.
    const ClipAuthoring authoring = ReadAuthoringFromStub(metaFileData.assetsPath);

    for (const AnimClipData& clip : model->clips)
    {
        if (clip.name == clipName)
            return SaveClip(metaFileData, clip, authoring);
    }

    NOUS_ERROR("ImporterAnimation: '%s' no longer contains a clip named '%s'.",
               source.c_str(), clipName.c_str());
    return false;
}

bool ImporterAnimation::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    const auto* animation = down_cast<ResourceAnimation*>(inResource);

    ClipAuthoring authoring;
    authoring.settings = animation->settings;
    authoring.events   = animation->events;

    return SaveClip(metaFileData, animation->clip, authoring);
}

void ImporterAnimation::Evict(ResourceBase* resource)
{
    auto* animation = down_cast<ResourceAnimation*>(resource);

    animation->clip = AnimClipData{};

    // Settings and events go back to the default too: an evicted slot must not hand
    // the next Deserialize a stale loop flag or a marker list from another clip if
    // that read fails partway.
    animation->settings = AnimationSettings{};
    animation->events.clear();
}

bool ImporterAnimation::Upload(ResourceBase*, IGPUResourceFactory*) { return true; }

void ImporterAnimation::Release(ResourceBase*, IGPUResourceFactory*) {}

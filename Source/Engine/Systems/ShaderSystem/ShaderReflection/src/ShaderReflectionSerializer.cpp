#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionSerializer.h"
#include <Logger/Logger.h>

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

// ---------------------------------------------------------------------------
// Write helpers
// ---------------------------------------------------------------------------

static JsonObject SerializeMember(const ReflectedMember& m)
{
    JsonObject o;
    o.Set("name",       m.name);
    o.Set("type",       static_cast<double>(m.type));
    o.Set("offset",     static_cast<double>(m.offset));
    o.Set("size",       static_cast<double>(m.size));
    o.Set("arrayCount", static_cast<double>(m.arrayCount));
    return o;
}

static JsonObject SerializeBinding(const ReflectedBinding& b)
{
    JsonObject o;
    o.Set("set",       static_cast<double>(b.set));
    o.Set("binding",   static_cast<double>(b.binding));
    o.Set("type",      static_cast<double>(b.type));
    o.Set("count",     static_cast<double>(b.count));
    o.Set("name",      b.name);
    o.Set("stageMask", static_cast<double>(b.stageMask));
    o.Set("blockSize", static_cast<double>(b.blockSize));

    JsonArray membersArr;
    for (const auto& m : b.members)
        membersArr.Append(SerializeMember(m));
    o.Set("members", std::move(membersArr));
    return o;
}

static JsonObject SerializePushConstant(const ReflectedPushConstant& pc)
{
    JsonObject o;
    o.Set("name",      pc.name);
    o.Set("offset",    static_cast<double>(pc.offset));
    o.Set("size",      static_cast<double>(pc.size));
    o.Set("stageMask", static_cast<double>(pc.stageMask));

    JsonArray membersArr;
    for (const auto& m : pc.members)
        membersArr.Append(SerializeMember(m));
    o.Set("members", std::move(membersArr));
    return o;
}

static JsonObject SerializeVertexInput(const ReflectedInput& vi)
{
    JsonObject o;
    o.Set("location",   static_cast<double>(vi.location));
    o.Set("name",       vi.name);
    o.Set("components", static_cast<double>(vi.components));
    o.Set("bitWidth",   static_cast<double>(vi.bitWidth));
    o.Set("sizeBytes",  static_cast<double>(vi.sizeBytes));
    o.Set("scalarType", static_cast<double>(vi.scalarType));
    return o;
}

static JsonObject SerializeFragmentOutput(const ReflectedOutput& fo)
{
    JsonObject o;
    o.Set("location",   static_cast<double>(fo.location));
    o.Set("name",       fo.name);
    o.Set("components", static_cast<double>(fo.components));
    o.Set("bitWidth",   static_cast<double>(fo.bitWidth));
    o.Set("scalarType", static_cast<double>(fo.scalarType));
    return o;
}

// ---------------------------------------------------------------------------
// Read helpers
// ---------------------------------------------------------------------------

static ReflectedMember DeserializeMember(const JsonObject& o)
{
    ReflectedMember m;
    m.name       = o.GetString("name");
    m.type       = static_cast<DataType>(static_cast<uint8_t>(o.GetDouble("type",       0.0)));
    m.offset     = static_cast<uint32_t>(o.GetDouble("offset",     0.0));
    m.size       = static_cast<uint32_t>(o.GetDouble("size",       0.0));
    m.arrayCount = static_cast<uint32_t>(o.GetDouble("arrayCount", 0.0));
    return m;
}

static ReflectedBinding DeserializeBinding(const JsonObject& o)
{
    ReflectedBinding b;
    b.set       = static_cast<uint32_t>(o.GetDouble("set",       0.0));
    b.binding   = static_cast<uint32_t>(o.GetDouble("binding",   0.0));
    b.type      = static_cast<DescriptorType>(static_cast<uint8_t>(o.GetDouble("type", 0.0)));
    b.count     = static_cast<uint32_t>(o.GetDouble("count",     0.0));
    b.name      = o.GetString("name");
    b.stageMask = static_cast<uint32_t>(o.GetDouble("stageMask", 0.0));
    b.blockSize = static_cast<uint32_t>(o.GetDouble("blockSize", 0.0));

    JsonArray members = o.GetArray("members");
    if (!members.IsEmpty())
    {
        const int count = members.Count();
        b.members.reserve(count);
        for (int i = 0; i < count; ++i)
            b.members.push_back(DeserializeMember(members.GetObject(i)));
    }
    return b;
}

static ReflectedPushConstant DeserializePushConstant(const JsonObject& o)
{
    ReflectedPushConstant pc;
    pc.name      = o.GetString("name");
    pc.offset    = static_cast<uint32_t>(o.GetDouble("offset",    0.0));
    pc.size      = static_cast<uint32_t>(o.GetDouble("size",      0.0));
    pc.stageMask = static_cast<uint32_t>(o.GetDouble("stageMask", 0.0));

    JsonArray members = o.GetArray("members");
    if (!members.IsEmpty())
    {
        const int count = members.Count();
        pc.members.reserve(count);
        for (int i = 0; i < count; ++i)
            pc.members.push_back(DeserializeMember(members.GetObject(i)));
    }
    return pc;
}

static ReflectedInput DeserializeVertexInput(const JsonObject& o)
{
    ReflectedInput vi;
    vi.location   = static_cast<uint32_t>(o.GetDouble("location",   0.0));
    vi.name       = o.GetString("name");
    vi.components = static_cast<uint8_t>(o.GetDouble("components", 0.0));
    vi.bitWidth   = static_cast<uint8_t>(o.GetDouble("bitWidth",   0.0));
    vi.sizeBytes  = static_cast<uint32_t>(o.GetDouble("sizeBytes",  0.0));
    vi.scalarType = static_cast<ScalarType>(static_cast<uint8_t>(o.GetDouble("scalarType", 0.0)));
    return vi;
}

static ReflectedOutput DeserializeFragmentOutput(const JsonObject& o)
{
    ReflectedOutput fo;
    fo.location   = static_cast<uint32_t>(o.GetDouble("location",   0.0));
    fo.name       = o.GetString("name");
    fo.components = static_cast<uint8_t>(o.GetDouble("components", 0.0));
    fo.bitWidth   = static_cast<uint8_t>(o.GetDouble("bitWidth",   0.0));
    fo.scalarType = static_cast<ScalarType>(static_cast<uint8_t>(o.GetDouble("scalarType", 0.0)));
    return fo;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool nous::engine::shader_system::SerializeReflection(const PipelineReflectionResult& reflection,
                                            const std::string& jsonPath)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Serializing reflection to '%s'", jsonPath.c_str());

    JsonObject root;

    // --- descriptorSets: array of { set, bindings[] } ---
    JsonArray dsetsArr;
    for (const auto& [setIndex, bindings] : reflection.descriptorSets)
    {
        JsonObject setObj;
        setObj.Set("set", static_cast<double>(setIndex));

        JsonArray bindingsArr;
        for (const auto& b : bindings)
            bindingsArr.Append(SerializeBinding(b));
        setObj.Set("bindings", std::move(bindingsArr));

        dsetsArr.Append(std::move(setObj));
    }
    root.Set("descriptorSets", std::move(dsetsArr));

    // --- pushConstants ---
    JsonArray pcArr;
    for (const auto& pc : reflection.pushConstants)
        pcArr.Append(SerializePushConstant(pc));
    root.Set("pushConstants", std::move(pcArr));

    // --- vertexInputs ---
    JsonArray viArr;
    for (const auto& vi : reflection.vertexInputs)
        viArr.Append(SerializeVertexInput(vi));
    root.Set("vertexInputs", std::move(viArr));

    // --- fragmentOutputs ---
    JsonArray foArr;
    for (const auto& fo : reflection.fragmentOutputs)
        foArr.Append(SerializeFragmentOutput(fo));
    root.Set("fragmentOutputs", std::move(foArr));

    const bool ok = JsonFile::SaveToFile(root, jsonPath);
    if (ok)
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Serialized reflection to '%s'", jsonPath.c_str());
    else
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to serialize reflection to '%s'", jsonPath.c_str());

    return ok;
}

bool nous::engine::shader_system::DeserializeReflection(const std::string& jsonPath,
                                              PipelineReflectionResult& outReflection)
{
    JsonObject root = JsonFile::LoadFromFile(jsonPath);
    if (root.IsEmpty())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to parse reflection JSON '%s'", jsonPath.c_str());
        return false;
    }

    // --- descriptorSets ---
    JsonArray dsets = root.GetArray("descriptorSets");
    if (!dsets.IsEmpty())
    {
        const int setCount = dsets.Count();
        for (int i = 0; i < setCount; ++i)
        {
            JsonObject setObj   = dsets.GetObject(i);
            auto setIndex = static_cast<uint32_t>(setObj.GetDouble("set", 0.0));

            std::vector<ReflectedBinding>& bindings = outReflection.descriptorSets[setIndex];
            JsonArray bindingsArr = setObj.GetArray("bindings");
            if (!bindingsArr.IsEmpty())
            {
                const int bCount = bindingsArr.Count();
                bindings.reserve(bCount);
                for (int j = 0; j < bCount; ++j)
                    bindings.push_back(DeserializeBinding(bindingsArr.GetObject(j)));
            }
        }
    }

    // --- pushConstants ---
    JsonArray pcs = root.GetArray("pushConstants");
    if (!pcs.IsEmpty())
    {
        const int count = pcs.Count();
        outReflection.pushConstants.reserve(count);
        for (int i = 0; i < count; ++i)
            outReflection.pushConstants.push_back(DeserializePushConstant(pcs.GetObject(i)));
    }

    // --- vertexInputs ---
    JsonArray vis = root.GetArray("vertexInputs");
    if (!vis.IsEmpty())
    {
        const int count = vis.Count();
        outReflection.vertexInputs.reserve(count);
        for (int i = 0; i < count; ++i)
            outReflection.vertexInputs.push_back(DeserializeVertexInput(vis.GetObject(i)));
    }

    // --- fragmentOutputs ---
    JsonArray fos = root.GetArray("fragmentOutputs");
    if (!fos.IsEmpty())
    {
        const int count = fos.Count();
        outReflection.fragmentOutputs.reserve(count);
        for (int i = 0; i < count; ++i)
            outReflection.fragmentOutputs.push_back(DeserializeFragmentOutput(fos.GetObject(i)));
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "Deserialized reflection from '%s'", jsonPath.c_str());
    return true;
}

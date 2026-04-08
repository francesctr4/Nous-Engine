#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionSerializer.h"
#include "Engine/Core/Logger/Logger.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

#include <parson.h>

// ---------------------------------------------------------------------------
// Write helpers
// ---------------------------------------------------------------------------

static JSON_Value* SerializeMember(const ReflectedMember& m)
{
    JSON_Value*  v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_string(o, "name",       m.name.c_str());
    json_object_set_number(o, "type",       static_cast<double>(m.type));
    json_object_set_number(o, "offset",     m.offset);
    json_object_set_number(o, "size",       m.size);
    json_object_set_number(o, "arrayCount", m.arrayCount);
    return v;
}

static JSON_Value* SerializeBinding(const ReflectedBinding& b)
{
    JSON_Value*  v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_number(o, "set",       b.set);
    json_object_set_number(o, "binding",   b.binding);
    json_object_set_number(o, "type",      static_cast<double>(b.type));
    json_object_set_number(o, "count",     b.count);
    json_object_set_string(o, "name",      b.name.c_str());
    json_object_set_number(o, "stageMask", b.stageMask);
    json_object_set_number(o, "blockSize", b.blockSize);

    JSON_Value* membersArr = json_value_init_array();
    JSON_Array* mArr       = json_value_get_array(membersArr);
    for (const auto& m : b.members)
        json_array_append_value(mArr, SerializeMember(m));
    json_object_set_value(o, "members", membersArr);

    return v;
}

static JSON_Value* SerializePushConstant(const ReflectedPushConstant& pc)
{
    JSON_Value*  v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_string(o, "name",      pc.name.c_str());
    json_object_set_number(o, "offset",    pc.offset);
    json_object_set_number(o, "size",      pc.size);
    json_object_set_number(o, "stageMask", pc.stageMask);

    JSON_Value* membersArr = json_value_init_array();
    JSON_Array* mArr       = json_value_get_array(membersArr);
    for (const auto& m : pc.members)
        json_array_append_value(mArr, SerializeMember(m));
    json_object_set_value(o, "members", membersArr);

    return v;
}

static JSON_Value* SerializeVertexInput(const ReflectedInput& vi)
{
    JSON_Value*  v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_number(o, "location",   vi.location);
    json_object_set_string(o, "name",       vi.name.c_str());
    json_object_set_number(o, "components", vi.components);
    json_object_set_number(o, "bitWidth",   vi.bitWidth);
    json_object_set_number(o, "sizeBytes",  vi.sizeBytes);
    json_object_set_number(o, "scalarType", static_cast<double>(vi.scalarType));
    return v;
}

static JSON_Value* SerializeFragmentOutput(const ReflectedOutput& fo)
{
    JSON_Value*  v = json_value_init_object();
    JSON_Object* o = json_value_get_object(v);
    json_object_set_number(o, "location",   fo.location);
    json_object_set_string(o, "name",       fo.name.c_str());
    json_object_set_number(o, "components", fo.components);
    json_object_set_number(o, "bitWidth",   fo.bitWidth);
    json_object_set_number(o, "scalarType", static_cast<double>(fo.scalarType));
    return v;
}

// ---------------------------------------------------------------------------
// Read helpers
// ---------------------------------------------------------------------------

static ReflectedMember DeserializeMember(const JSON_Object* o)
{
    ReflectedMember m;
    if (const char* name = json_object_get_string(o, "name")) m.name = name;
    m.type       = static_cast<DataType>(static_cast<uint8_t>(json_object_get_number(o, "type")));
    m.offset     = static_cast<uint32_t>(json_object_get_number(o, "offset"));
    m.size       = static_cast<uint32_t>(json_object_get_number(o, "size"));
    m.arrayCount = static_cast<uint32_t>(json_object_get_number(o, "arrayCount"));
    return m;
}

static ReflectedBinding DeserializeBinding(const JSON_Object* o)
{
    ReflectedBinding b;
    b.set       = static_cast<uint32_t>(json_object_get_number(o, "set"));
    b.binding   = static_cast<uint32_t>(json_object_get_number(o, "binding"));
    b.type      = static_cast<DescriptorType>(static_cast<uint8_t>(json_object_get_number(o, "type")));
    b.count     = static_cast<uint32_t>(json_object_get_number(o, "count"));
    if (const char* name = json_object_get_string(o, "name")) b.name = name;
    b.stageMask = static_cast<uint32_t>(json_object_get_number(o, "stageMask"));
    b.blockSize = static_cast<uint32_t>(json_object_get_number(o, "blockSize"));

    if (const JSON_Array* members = json_object_get_array(o, "members"))
    {
        const size_t count = json_array_get_count(members);
        b.members.reserve(count);
        for (size_t i = 0; i < count; ++i)
            b.members.push_back(DeserializeMember(json_array_get_object(members, i)));
    }
    return b;
}

static ReflectedPushConstant DeserializePushConstant(const JSON_Object* o)
{
    ReflectedPushConstant pc;
    if (const char* name = json_object_get_string(o, "name")) pc.name = name;
    pc.offset    = static_cast<uint32_t>(json_object_get_number(o, "offset"));
    pc.size      = static_cast<uint32_t>(json_object_get_number(o, "size"));
    pc.stageMask = static_cast<uint32_t>(json_object_get_number(o, "stageMask"));

    if (const JSON_Array* members = json_object_get_array(o, "members"))
    {
        const size_t count = json_array_get_count(members);
        pc.members.reserve(count);
        for (size_t i = 0; i < count; ++i)
            pc.members.push_back(DeserializeMember(json_array_get_object(members, i)));
    }
    return pc;
}

static ReflectedInput DeserializeVertexInput(const JSON_Object* o)
{
    ReflectedInput vi;
    vi.location   = static_cast<uint32_t>(json_object_get_number(o, "location"));
    if (const char* name = json_object_get_string(o, "name")) vi.name = name;
    vi.components = static_cast<uint8_t>(json_object_get_number(o, "components"));
    vi.bitWidth   = static_cast<uint8_t>(json_object_get_number(o, "bitWidth"));
    vi.sizeBytes  = static_cast<uint32_t>(json_object_get_number(o, "sizeBytes"));
    vi.scalarType = static_cast<ScalarType>(static_cast<uint8_t>(json_object_get_number(o, "scalarType")));
    return vi;
}

static ReflectedOutput DeserializeFragmentOutput(const JSON_Object* o)
{
    ReflectedOutput fo;
    fo.location   = static_cast<uint32_t>(json_object_get_number(o, "location"));
    if (const char* name = json_object_get_string(o, "name")) fo.name = name;
    fo.components = static_cast<uint8_t>(json_object_get_number(o, "components"));
    fo.bitWidth   = static_cast<uint8_t>(json_object_get_number(o, "bitWidth"));
    fo.scalarType = static_cast<ScalarType>(static_cast<uint8_t>(json_object_get_number(o, "scalarType")));
    return fo;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool NOUS_ShaderSystem::SerializeReflection(const PipelineReflectionResult& reflection,
                                            const std::string& jsonPath)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Serializing reflection to '%s'", jsonPath.c_str());

    JSON_Value*  rootVal = json_value_init_object();
    JSON_Object* root    = json_value_get_object(rootVal);

    // --- descriptorSets: array of { set, bindings[] } ---
    JSON_Value* dsetsArr = json_value_init_array();
    JSON_Array* dsets    = json_value_get_array(dsetsArr);

    for (const auto& [setIndex, bindings] : reflection.descriptorSets)
    {
        JSON_Value*  setVal = json_value_init_object();
        JSON_Object* setObj = json_value_get_object(setVal);
        json_object_set_number(setObj, "set", setIndex);

        JSON_Value* bindingsArr = json_value_init_array();
        JSON_Array* bArr        = json_value_get_array(bindingsArr);
        for (const auto& b : bindings)
            json_array_append_value(bArr, SerializeBinding(b));
        json_object_set_value(setObj, "bindings", bindingsArr);

        json_array_append_value(dsets, setVal);
    }
    json_object_set_value(root, "descriptorSets", dsetsArr);

    // --- pushConstants ---
    JSON_Value* pcArr = json_value_init_array();
    JSON_Array* pcs   = json_value_get_array(pcArr);
    for (const auto& pc : reflection.pushConstants)
        json_array_append_value(pcs, SerializePushConstant(pc));
    json_object_set_value(root, "pushConstants", pcArr);

    // --- vertexInputs ---
    JSON_Value* viArr = json_value_init_array();
    JSON_Array* vis   = json_value_get_array(viArr);
    for (const auto& vi : reflection.vertexInputs)
        json_array_append_value(vis, SerializeVertexInput(vi));
    json_object_set_value(root, "vertexInputs", viArr);

    // --- fragmentOutputs ---
    JSON_Value* foArr = json_value_init_array();
    JSON_Array* fos   = json_value_get_array(foArr);
    for (const auto& fo : reflection.fragmentOutputs)
        json_array_append_value(fos, SerializeFragmentOutput(fo));
    json_object_set_value(root, "fragmentOutputs", foArr);

    const bool ok = json_serialize_to_file_pretty(rootVal, jsonPath.c_str()) == JSONSuccess;
    json_value_free(rootVal);

    if (ok)
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Serialized reflection to '%s'", jsonPath.c_str());
    else
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to serialize reflection to '%s'", jsonPath.c_str());

    return ok;
}

bool NOUS_ShaderSystem::DeserializeReflection(const std::string& jsonPath,
                                              PipelineReflectionResult& outReflection)
{
    JSON_Value* rootVal = json_parse_file(jsonPath.c_str());
    if (!rootVal)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to parse reflection JSON '%s'", jsonPath.c_str());
        return false;
    }

    const JSON_Object* root = json_value_get_object(rootVal);
    if (!root) { json_value_free(rootVal); return false; }

    // --- descriptorSets ---
    if (const JSON_Array* dsets = json_object_get_array(root, "descriptorSets"))
    {
        const size_t setCount = json_array_get_count(dsets);
        for (size_t i = 0; i < setCount; ++i)
        {
            const JSON_Object* setObj   = json_array_get_object(dsets, i);
            auto     setIndex = static_cast<uint32_t>(json_object_get_number(setObj, "set"));

            std::vector<ReflectedBinding>& bindings = outReflection.descriptorSets[setIndex];
            if (const JSON_Array* bindingsArr = json_object_get_array(setObj, "bindings"))
            {
                const size_t bCount = json_array_get_count(bindingsArr);
                bindings.reserve(bCount);
                for (size_t j = 0; j < bCount; ++j)
                    bindings.push_back(DeserializeBinding(json_array_get_object(bindingsArr, j)));
            }
        }
    }

    // --- pushConstants ---
    if (const JSON_Array* pcs = json_object_get_array(root, "pushConstants"))
    {
        const size_t count = json_array_get_count(pcs);
        outReflection.pushConstants.reserve(count);
        for (size_t i = 0; i < count; ++i)
            outReflection.pushConstants.push_back(
                DeserializePushConstant(json_array_get_object(pcs, i)));
    }

    // --- vertexInputs ---
    if (const JSON_Array* vis = json_object_get_array(root, "vertexInputs"))
    {
        const size_t count = json_array_get_count(vis);
        outReflection.vertexInputs.reserve(count);
        for (size_t i = 0; i < count; ++i)
            outReflection.vertexInputs.push_back(
                DeserializeVertexInput(json_array_get_object(vis, i)));
    }

    // --- fragmentOutputs ---
    if (const JSON_Array* fos = json_object_get_array(root, "fragmentOutputs"))
    {
        const size_t count = json_array_get_count(fos);
        outReflection.fragmentOutputs.reserve(count);
        for (size_t i = 0; i < count; ++i)
            outReflection.fragmentOutputs.push_back(
                DeserializeFragmentOutput(json_array_get_object(fos, i)));
    }

    json_value_free(rootVal);
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Deserialized reflection from '%s'", jsonPath.c_str());
    return true;
}

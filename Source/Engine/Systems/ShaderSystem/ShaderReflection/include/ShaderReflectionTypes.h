#ifndef NOUS_ENGINE_SHADER_REFLECTION_TYPES_H
#define NOUS_ENGINE_SHADER_REFLECTION_TYPES_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class DescriptorType : uint8_t
{
    Unknown = 0,
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    SampledImage,
    Sampler,
    StorageImage
};

enum class DataType : uint8_t {
    Unknown,
    // scalars
    Bool, Int, UInt, Float,
    // vectors
    Vec2, Vec3, Vec4,
    IVec2, IVec3, IVec4,
    UVec2, UVec3, UVec4,
    // matrices
    Mat2, Mat3, Mat4,
  };

struct ReflectedMember {
    std::string name;
    DataType type;
    uint32_t offset;
    uint32_t size;      // bytes
    uint32_t arrayCount; // 1 if not array
};

struct ReflectedBinding
{
    uint32_t set = 0;
    uint32_t binding = 0;
    DescriptorType type = DescriptorType::Unknown;
    uint32_t count = 1;             // arrays: e.g., sampler2D tex[4]
    std::string name;               // "CameraUBO", "uAlbedo", etc.
    uint32_t stageMask;
    uint32_t blockSize;

    std::vector<ReflectedMember> members; // only for UBO/SSBO
};

struct ReflectedPushConstant
{
    uint32_t offset = 0;
    uint32_t size = 0;
    std::string name;
    uint32_t stageMask;

    std::vector<ReflectedMember> members;   // opcional pero potente
};

enum class ScalarType : uint8_t { Unknown, Bool, Int, UInt, Float };

struct ReflectedInput
{
    uint32_t location = 0;
    std::string name;               // aPos, aUV...
    // Later: format, vec size, etc.
    uint8_t     components = 0;               // 1..4 (vecN)
    uint8_t     bitWidth = 0;                 // 8/16/32/64 (normalmente 32)
    uint32_t    sizeBytes = 0;
    ScalarType scalarType;

    // Convenience: misma semántica que ReflectedMember::type
    DataType ToDataType() const
    {
        if (scalarType == ScalarType::Float)
        {
            switch (components) {
                case 1: return DataType::Float;
                case 2: return DataType::Vec2;
                case 3: return DataType::Vec3;
                case 4: return DataType::Vec4;
            }
        }
        if (scalarType == ScalarType::Int)
        {
            switch (components) {
                case 1: return DataType::Int;
                case 2: return DataType::IVec2;
                case 3: return DataType::IVec3;
                case 4: return DataType::IVec4;
            }
        }
        if (scalarType == ScalarType::UInt)
        {
            switch (components) {
                case 1: return DataType::UInt;
                case 2: return DataType::UVec2;
                case 3: return DataType::UVec3;
                case 4: return DataType::UVec4;
            }
        }
        if (scalarType == ScalarType::Bool)  return DataType::Bool;

        return DataType::Unknown;
    }
};

struct ReflectedOutput
{
    uint32_t   location = 0;
    std::string name;
    uint8_t    components = 0;
    uint8_t    bitWidth   = 0;
    ScalarType scalarType = ScalarType::Unknown;
};

struct ShaderReflectionResult
{
    bool success = false;
    std::string errorMessage;

    std::vector<ReflectedBinding> bindings;
    std::vector<ReflectedPushConstant> pushConstants;
    std::vector<ReflectedInput> vertexInputs;
    std::vector<ReflectedOutput> fragmentOutputs;
};

struct PipelineReflectionResult
{
    // set → bindings (con stageMask ya combinado)
    std::unordered_map<uint32_t, std::vector<ReflectedBinding>> descriptorSets;

    std::vector<ReflectedPushConstant> pushConstants; // también merged
    std::vector<ReflectedInput>        vertexInputs;
    std::vector<ReflectedOutput>       fragmentOutputs; // ver punto 2
};

#endif //NOUS_ENGINE_SHADERREFLECTIONTYPES_H
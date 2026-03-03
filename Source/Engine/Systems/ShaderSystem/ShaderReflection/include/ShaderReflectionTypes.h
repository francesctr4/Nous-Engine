#ifndef NOUS_ENGINE_SHADER_REFLECTION_TYPES_H
#define NOUS_ENGINE_SHADER_REFLECTION_TYPES_H

#include <cstdint>
#include <string>
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
    uint32_t binding;
};

struct ShaderReflectionResult
{
    bool success = false;
    std::string errorMessage;

    std::vector<ReflectedBinding> bindings;
    std::vector<ReflectedPushConstant> pushConstants;
    std::vector<ReflectedInput> vertexInputs;
};

#endif //NOUS_ENGINE_SHADERREFLECTIONTYPES_H
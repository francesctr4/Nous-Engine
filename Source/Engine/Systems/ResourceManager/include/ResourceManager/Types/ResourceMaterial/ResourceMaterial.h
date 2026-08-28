#ifndef RESOURCEMATERIAL_H
#define RESOURCEMATERIAL_H

#include <ResourceManager/Core/ResourceBase.h>
#include <EngineCore/InvalidID.h>
#include <Renderer/RendererTypes.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>

#include "glm/glm.hpp"
#include <cstdint>
#include <unordered_map>
#include <string>

class ResourceTexture;
class ResourceShader;

enum class UniformValueType : uint8_t
{
    Float, Vec2, Vec3, Vec4,
    Int, IVec2, IVec3, IVec4
};

// ---------------------------------------------------------------------------
// UniformValue helpers — shared by renderer, importer, and editor.
// Defined here so no consumer needs to duplicate them.
// ---------------------------------------------------------------------------

inline uint32_t UniformValueComponentCount(UniformValueType type)
{
    switch (type)
    {
        case UniformValueType::Float: case UniformValueType::Int:   return 1;
        case UniformValueType::Vec2:  case UniformValueType::IVec2: return 2;
        case UniformValueType::Vec3:  case UniformValueType::IVec3: return 3;
        case UniformValueType::Vec4:  case UniformValueType::IVec4: return 4;
    }
    return 4;
}

inline bool UniformValueIsInt(UniformValueType type)
{
    return type == UniformValueType::Int   || type == UniformValueType::IVec2
        || type == UniformValueType::IVec3 || type == UniformValueType::IVec4;
}

inline UniformValueType DataTypeToUniformValueType(DataType dt)
{
    switch (dt)
    {
        case DataType::Float: return UniformValueType::Float;
        case DataType::Vec2:  return UniformValueType::Vec2;
        case DataType::Vec3:  return UniformValueType::Vec3;
        case DataType::Vec4:  return UniformValueType::Vec4;
        case DataType::Int:   return UniformValueType::Int;
        case DataType::IVec2: return UniformValueType::IVec2;
        case DataType::IVec3: return UniformValueType::IVec3;
        case DataType::IVec4: return UniformValueType::IVec4;
        default:              return UniformValueType::Vec4;
    }
}

// ---------------------------------------------------------------------------

struct UniformValue
{
    UniformValueType type = UniformValueType::Vec4;

    // Split storage: float types use fdata, int types use idata.
    // Always access the field that matches 'type' — reading the wrong member
    // gives garbage (different bit patterns: 1.0f = 0x3F800000, 1 = 0x00000001).
    union
    {
        glm::vec4  fdata = glm::vec4(1.0f);  // Float / Vec2 / Vec3 / Vec4
        glm::ivec4 idata;                     // Int   / IVec2 / IVec3 / IVec4
    };

    // Returns a properly-initialised default value (1.0f for floats, 1 for ints).
    static UniformValue MakeDefault(UniformValueType t)
    {
        UniformValue uv;
        uv.type = t;
        if (UniformValueIsInt(t))
            uv.idata = glm::ivec4(1);
        // else: fdata already initialised to glm::vec4(1.0f) by the default member initialiser
        return uv;
    }
};

class ResourceMaterial : public ResourceBase
{
public:

	// Constructor & Destructor

	NOUS_ENGINE_API explicit ResourceMaterial(uint32_t uid = 0);
	NOUS_ENGINE_API ~ResourceMaterial() override;

    // Sets shader + shaderUID and clears poolOwnerShader in one atomic step.
    // Always use this instead of assigning shader directly — it prevents stale
    // poolOwnerShader from pointing into the old shader's instance pool after a
    // shader change. VulkanBackend::CreateMaterial re-derives poolOwnerShader.
    NOUS_ENGINE_API void SetShader(ResourceShader* newShader);

public:

    // GPU-side descriptor set instance slot index inside the owning shader's pool.
    // Set by VulkanBackend::CreateMaterial, cleared to INVALID_ID on destroy.
    // Used every frame to bind the correct per-material descriptor set.
    uint32_t internalID;

    std::unordered_map<std::string, TextureMap>   textureMaps;   // key = GLSL binding name (e.g. "diffuseSampler")
    std::unordered_map<std::string, UniformValue> uniformValues; // key = GLSL InstanceUBO member name

    // -----------------------------------------------------------------------
    // Shader ownership rules
    //
    // shader          — set via SetShader() by ImporterMaterial::DeserializeShader
    //                   (CPU side). null = use built-in MaterialShader.
    //
    // shaderUID       — serialisation key; kept in sync with shader by SetShader().
    //
    // poolOwnerShader — GPU-only; set by VulkanBackend::CreateMaterial to record
    //                   which shader's instance pool owns 'internalID'.
    //                   Cleared by VulkanBackend::DestroyMaterial and by SetShader().
    //                   Never set outside the renderer backend.
    // -----------------------------------------------------------------------
    ResourceShader* shader          = nullptr;    // null = use built-in MaterialShader
    uint32_t          shaderUID       = INVALID_ID; // for serialisation / reload
    ResourceShader* poolOwnerShader = nullptr;    // runtime only — which shader's pool owns internalID

};

#endif // RESOURCEMATERIAL_H

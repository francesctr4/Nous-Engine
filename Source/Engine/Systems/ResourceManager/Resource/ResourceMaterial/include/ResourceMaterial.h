#ifndef RESOURCEMATERIAL_H
#define RESOURCEMATERIAL_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Renderer/RendererTypes.h"

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

struct UniformValue
{
    UniformValueType type = UniformValueType::Vec4;
    glm::vec4 data = glm::vec4(1.0f); // universal storage; interpret by type (ints reinterpreted at upload)
};

class ResourceMaterial : public Resource
{
public:

	// Constructor & Destructor

	explicit ResourceMaterial(UID uid = 0);
	~ResourceMaterial() override;

public:

    uint32 ID;
    uint32 internalID;
    uint32 generation;

    std::unordered_map<std::string, TextureMap>   textureMaps;   // key = GLSL binding name (e.g. "diffuseSampler")
    std::unordered_map<std::string, UniformValue> uniformValues; // key = GLSL InstanceUBO member name

    ResourceShader* shader          = nullptr;    // null = use built-in MaterialShader
    uint32          shaderUID       = INVALID_ID; // for serialisation / reload
    ResourceShader* poolOwnerShader = nullptr;    // runtime only — which shader's pool owns internalID

};

#endif // RESOURCEMATERIAL_H
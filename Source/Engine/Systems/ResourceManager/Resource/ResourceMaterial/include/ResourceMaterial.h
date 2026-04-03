#ifndef RESOURCEMATERIAL_H
#define RESOURCEMATERIAL_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Renderer/RendererTypes.h"

#include "glm/glm.hpp"
#include <unordered_map>
#include <string>

class ResourceTexture;
class ResourceShader;

class ResourceMaterial : public Resource
{
public:

	// Constructor & Destructor

	ResourceMaterial(UID uid = 0);
	~ResourceMaterial() override;

public:

    uint32 ID;
    uint32 internalID;
    uint32 generation;

    std::unordered_map<std::string, TextureMap> textureMaps; // key = GLSL binding name (e.g. "diffuseSampler")
    glm::vec4  diffuseColor;

    ResourceShader* shader    = nullptr;    // null = use built-in MaterialShader
    uint32          shaderUID = INVALID_ID; // for serialisation / reload

};

#endif // RESOURCEMATERIAL_H
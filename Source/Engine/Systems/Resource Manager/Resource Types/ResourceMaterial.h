#ifndef RESOURCEMATERIAL_H
#define RESOURCEMATERIAL_H

#include "Engine/Core/Globals.h"
#include "Resource.h"
#include "Engine/Utils/MathUtils.h"

#include "glm/glm.hpp"

class ResourceTexture;

enum class TextureMapType
{
    UNKNOWN = -1,
    DIFFUSE
};

struct TextureMap
{
    TextureMap() : type(TextureMapType::UNKNOWN), texture(nullptr) {}

    TextureMapType type;
    ResourceTexture* texture;
};

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

    TextureMap diffuseMap;
    glm::vec4 diffuseColor;

};

#endif // RESOURCEMATERIAL_H
#ifndef RESOURCEMATERIAL_H
#define RESOURCEMATERIAL_H

#include "Core/Globals.h"
#include "Systems/Resource Manager/Resource Types/Resource.h"
#include "Utils/MathUtils.h"

#include "Includes/glmath.h"

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
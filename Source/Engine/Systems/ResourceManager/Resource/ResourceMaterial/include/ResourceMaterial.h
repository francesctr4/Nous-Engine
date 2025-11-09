#ifndef RESOURCEMATERIAL_H
#define RESOURCEMATERIAL_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Renderer/RendererTypes.h"

#include "glm/glm.hpp"

class ResourceTexture;

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
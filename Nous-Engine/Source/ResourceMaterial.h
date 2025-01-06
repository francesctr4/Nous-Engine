#pragma once

#include "Globals.h"
#include "Resource.h"
#include "MathUtils.h"

class ResourceTexture;

enum class TextureMapType2
{
    UNKNOWN = -1,
    DIFFUSE
};

struct TextureMap2
{
    TextureMapType2 type;
    ResourceTexture* texture;
};

class ResourceMaterial : public Resource
{
public:

	// Constructor & Destructor

	ResourceMaterial(UID uid = 0);
	~ResourceMaterial() override;

    void IncreaseReferenceCount() override;
    void DecreaseReferenceCount() override;

public:

    uint32 ID;
    uint32 internalID;
    uint32 generation;

    TextureMap2 diffuseMap;
    float4 diffuseColor;

};
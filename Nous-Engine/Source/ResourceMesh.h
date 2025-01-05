#pragma once

#include "Globals.h"
#include "Resource.h"

#include "RendererTypes.inl"

//class ResourceMaterial;

class ResourceMesh : public Resource
{
public:

	// Constructor & Destructor

	ResourceMesh(UID uid = 0);
	~ResourceMesh() override;

public:

	std::vector<Vertex> vertices;
	std::vector<uint32> indices;

	//ResourceMaterial* material;
};
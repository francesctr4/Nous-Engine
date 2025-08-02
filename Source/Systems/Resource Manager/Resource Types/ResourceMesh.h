#ifndef RESOURCEMESH_H
#define RESOURCEMESH_H

#include "Core/Globals.h"
#include "Systems/Resource Manager/Resource Types/Resource.h"
#include "Renderer/RendererTypes.inl"
#include "Systems/Geometry System/Vertex.inl"

class ResourceMaterial;

class ResourceMesh : public Resource
{
public:

	// Constructor & Destructor

	ResourceMesh(UID uid = 0);
	~ResourceMesh() override;

public:

	uint32 ID;
	uint32 internalID;
	uint32 generation;

	std::vector<Vertex3D> vertices;
	std::vector<uint32> indices;

	ResourceMaterial* material;
};

#endif // RESOURCEMESH_H
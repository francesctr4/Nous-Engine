#ifndef RESOURCEMESH_H
#define RESOURCEMESH_H

#include <Engine/Core/Globals.h>
#include <Engine/Systems/Resource Manager/Resource Types/Resource.h>
#include <Engine/Renderer/RendererTypes.h>
#include "Engine/Utils/Vertex.inl"

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
};

#endif // RESOURCEMESH_H
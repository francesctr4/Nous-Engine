#ifndef RESOURCEMESH_H
#define RESOURCEMESH_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Utils/Math/Vertex.inl"

#include <vector>

class ResourceMaterial;

class ResourceMesh : public Resource
{
public:

	// Constructor & Destructor

	NOUS_ENGINE_API ResourceMesh(uint32 uid = 0);
	NOUS_ENGINE_API ~ResourceMesh() override;

public:

	// GPU-side slot index into VulkanContext::geometries[].
	// Set by VulkanBackend::CreateGeometry, cleared to INVALID_ID on destroy.
	// Used every frame to locate the vertex/index buffer offsets for this mesh.
	uint32 internalID;

	std::vector<Vertex3D> vertices;
	std::vector<uint32> indices;
};

#endif // RESOURCEMESH_H
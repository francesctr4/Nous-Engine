#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"

ResourceMesh::ResourceMesh(uint32 uid) : Resource(uid, ResourceType::MESH)
{
	internalID = INVALID_ID;
}

ResourceMesh::~ResourceMesh()
{
}

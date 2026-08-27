#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>

ResourceMesh::ResourceMesh(uint32 uid) : ResourceBase(uid, ResourceType::MESH)
{
	internalID = INVALID_ID;
}

ResourceMesh::~ResourceMesh()
{
}

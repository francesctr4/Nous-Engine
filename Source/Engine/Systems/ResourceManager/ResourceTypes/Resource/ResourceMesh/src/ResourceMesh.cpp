#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceMesh/include/ResourceMesh.h"

ResourceMesh::ResourceMesh(uint32 uid) : Resource(uid, ResourceType::MESH)
{
	internalID = INVALID_ID;
}

ResourceMesh::~ResourceMesh()
{
}

#include "ResourceMesh.h"

ResourceMesh::ResourceMesh(UID uid) : Resource(uid, ResourceType::MESH)
{
	ID = INVALID_ID;
	internalID = INVALID_ID;
	generation = INVALID_ID;

	material = nullptr;
}

ResourceMesh::~ResourceMesh()
{
}

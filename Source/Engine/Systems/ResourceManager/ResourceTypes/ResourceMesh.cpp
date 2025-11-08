#include <Engine/Systems/ResourceManager/ResourceTypes/ResourceMesh.h>

ResourceMesh::ResourceMesh(UID uid) : Resource(uid, ResourceType::MESH)
{
	ID = INVALID_ID;
	internalID = INVALID_ID;
	generation = INVALID_ID;
}

ResourceMesh::~ResourceMesh()
{
}

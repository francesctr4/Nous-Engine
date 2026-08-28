#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <EngineCore/InvalidID.h>

ResourceMesh::ResourceMesh(uint32_t uid) : ResourceBase(uid, ResourceType::MESH)
{
	internalID = INVALID_ID;
}

ResourceMesh::~ResourceMesh()
{
}

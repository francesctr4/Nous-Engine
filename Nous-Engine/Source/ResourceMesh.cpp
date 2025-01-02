#include "ResourceMesh.h"

ResourceMesh::ResourceMesh(UID uid) : Resource(uid, ResourceType::MESH)
{

}

ResourceMesh::~ResourceMesh()
{

}

bool ResourceMesh::LoadInMemory()
{
	return false;
}

bool ResourceMesh::UnloadFromMemory()
{
	return false;
}

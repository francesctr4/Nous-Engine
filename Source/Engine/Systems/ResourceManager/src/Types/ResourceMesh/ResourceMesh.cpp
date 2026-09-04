#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <EngineCore/InvalidID.h>

ResourceMesh::ResourceMesh(uint32_t uid) : ResourceBase(uid, ResourceType::MESH)
{
	internalID = INVALID_ID;
}

ResourceMesh::~ResourceMesh()
{
}

void ResourceMesh::RecomputeDerivedData()
{
	hasSkinning = false;

	if (vertices.empty())
	{
		localAABBMin = glm::vec3(0.0f);
		localAABBMax = glm::vec3(0.0f);
		return;
	}

	localAABBMin = vertices[0].position;
	localAABBMax = vertices[0].position;

	for (const auto& v : vertices)
	{
		localAABBMin = glm::min(localAABBMin, v.position);
		localAABBMax = glm::max(localAABBMax, v.position);

		if (v.boneWeights.x != 0.0f || v.boneWeights.y != 0.0f ||
		    v.boneWeights.z != 0.0f || v.boneWeights.w != 0.0f)
		{
			hasSkinning = true;
		}
	}
}

#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <EngineCore/InvalidID.h>

#include <limits>

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
	boneAABBMin.clear();
	boneAABBMax.clear();

	if (vertices.empty())
	{
		localAABBMin = glm::vec3(0.0f);
		localAABBMax = glm::vec3(0.0f);
		return;
	}

	localAABBMin = vertices[0].position;
	localAABBMax = vertices[0].position;

	// One pass builds the whole-mesh box and hasSkinning; the per-bone boxes need
	// the highest bone index, which is only known once the pass is done, so they
	// grow on demand rather than in a second pass over the vertices.
	constexpr float k_inf = std::numeric_limits<float>::max();

	const auto expandBone = [this](uint32_t bone, const glm::vec3& p)
	{
		if (bone >= boneAABBMin.size())
		{
			// Inverted boxes: a bone that never gets a vertex stays inverted, which
			// is the "skip me" marker the bounds pass reads.
			boneAABBMin.resize(bone + 1, glm::vec3( k_inf));
			boneAABBMax.resize(bone + 1, glm::vec3(-k_inf));
		}
		boneAABBMin[bone] = glm::min(boneAABBMin[bone], p);
		boneAABBMax[bone] = glm::max(boneAABBMax[bone], p);
	};

	for (const auto& v : vertices)
	{
		localAABBMin = glm::min(localAABBMin, v.position);
		localAABBMax = glm::max(localAABBMax, v.position);

		if (v.boneWeights.x != 0.0f || v.boneWeights.y != 0.0f ||
		    v.boneWeights.z != 0.0f || v.boneWeights.w != 0.0f)
		{
			hasSkinning = true;

			// Only influences with weight are recorded. A zero-weight slot carries a
			// bone ID of 0 by default, so counting it would attach every unweighted
			// vertex in the mesh to bone 0 and inflate the root's box to the whole
			// character -- reintroducing the very problem per-bone boxes solve.
			if (v.boneWeights.x > 0.0f) expandBone(v.boneIDs.x, v.position);
			if (v.boneWeights.y > 0.0f) expandBone(v.boneIDs.y, v.position);
			if (v.boneWeights.z > 0.0f) expandBone(v.boneIDs.z, v.position);
			if (v.boneWeights.w > 0.0f) expandBone(v.boneIDs.w, v.position);
		}
	}
}

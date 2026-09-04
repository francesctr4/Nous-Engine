#pragma once

#include <ResourceManager/Core/ResourceBase.h>
#include <Utils/Math/Vertex.inl>

#include <vector>
#include <glm/glm.hpp>

class ResourceMaterial;

class ResourceMesh : public ResourceBase
{
public:

	// Constructor & Destructor

	NOUS_ENGINE_API ResourceMesh(uint32_t uid = 0);
	NOUS_ENGINE_API ~ResourceMesh() override;

public:

	// GPU-side slot index into VulkanContext::geometries[].
	// Set by VulkanBackend::CreateGeometry, cleared to INVALID_ID on destroy.
	// Used every frame to locate the vertex/index buffer offsets for this mesh.
	uint32_t internalID;

	std::vector<Vertex3D> vertices;
	std::vector<uint32_t> indices;

	// Local-space AABB computed once after import. Used every frame by the
	// AABB cache pass — avoids iterating all vertices per frame.
	glm::vec3 localAABBMin {0.f};
	glm::vec3 localAABBMax {0.f};

	// True when any vertex carries a non-zero bone weight — i.e. "is this mesh
	// rigged". Derived at load, never serialized: the mesh binary is a derived
	// cache, and adding a field would mean bumping the magic and regenerating all
	// of Library/ for a value that costs one pass over vertices we just read.
	//
	// All-zero weights are the "no influence" encoding every static mesh writes,
	// so the scan is exact rather than a heuristic.
	bool hasSkinning {false};

	// Recomputes everything derived from `vertices`: the local AABB and
	// hasSkinning. Call after assigning vertices, from every path that builds a
	// mesh. Safe on an empty mesh.
	NOUS_ENGINE_API void RecomputeDerivedData();
};
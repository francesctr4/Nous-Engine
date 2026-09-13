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

	// Hash of the bone NAMES of the skeleton this mesh was skinned against, 0 when the
	// source model had no skeleton. Unlike hasSkinning this is NOT derived -- it comes
	// from the mesh binary, because the vertices alone cannot say which rig they belong
	// to. Compared against ResourceSkeleton::nameHash at the animator/mesh pairing.
	uint64_t skeletonNameHash {0};

	// Per-bone bind-pose AABB: the box of the vertices each bone actually
	// influences, indexed by bone ID, empty for an unrigged mesh. Derived at load
	// like hasSkinning, never serialized.
	//
	// This is what makes the skinned bound usable. Transforming the WHOLE mesh box
	// by every bone matrix is also provably safe, but a hand bone then drags a
	// body-sized box out to the hand and the union ends up several times the
	// character -- correct, and useless for culling. Each bone carrying only its own
	// vertices keeps the union close to the real silhouette at the same cost.
	//
	// A bone with no influenced vertices keeps an INVERTED box (min > max) as its
	// "no vertices" marker, so it can be skipped without a parallel flag array.
	std::vector<glm::vec3> boneAABBMin;
	std::vector<glm::vec3> boneAABBMax;

	// Recomputes everything derived from `vertices`: the local AABB, hasSkinning
	// and the per-bone AABBs. Call after assigning vertices, from every path that
	// builds a mesh. Safe on an empty mesh.
	NOUS_ENGINE_API void RecomputeDerivedData();
};
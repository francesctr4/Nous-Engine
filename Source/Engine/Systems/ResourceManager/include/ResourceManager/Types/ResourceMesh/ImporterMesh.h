#ifndef IMPORTERMESH_H
#define IMPORTERMESH_H

#include <ResourceManager/Core/IImporter.h>
#include <Utils/Math/Vertex.inl>

#include <glm/glm.hpp>
#include <string>
#include <vector>

// One logical submesh extracted from an Assimp scene node.
// localTransform is the accumulated world transform from the scene root to this
// node (column-major, GLM convention). Vertices are in the submesh's local space.
// materialAssetPath is the Assets/-relative path to the generated .nmat sibling
// (empty when the assimp material had no usable texture slots, or the format's
// slots are not trusted). SpawnMeshAsHierarchy resolves it to a ResourceMaterial.
struct SubMeshData
{
    std::string         name;
    glm::mat4           localTransform { 1.0f };
    std::vector<Vertex3D>  vertices;
    std::vector<uint32_t>  indices;
    std::string         materialAssetPath;

    // Hash of the source model's bone NAMES, or 0 when the model has no skeleton.
    // Serialized so a mesh can be checked against the .nskel a CAnimator was given:
    // nothing else on disk or in memory says which rig a mesh is skinned to, and the
    // wrong pairing produces silently garbled geometry rather than an error.
    uint64_t            skeletonNameHash = 0;
};

// A submesh WITHOUT its geometry: everything the scene needs to build the
// GameObject hierarchy, and none of the megabytes it does not.
//
// This exists because reading one submesh used to mean deserializing all of them.
// SpawnMeshAsHierarchy called LoadHierarchy (full parse), then fanned out one
// SubMeshCache::RequestOrCreate per submesh across worker threads -- and each of
// those called LoadHierarchy again. An N-submesh model was parsed N+1 times, N of
// them concurrently: O(N^2) bytes read and vectors allocated to produce N meshes.
// The V4 offset directory plus this lighter type is what removes that.
struct SubMeshInfo
{
    std::string name;
    glm::mat4   localTransform{ 1.0f };
    std::string materialAssetPath;
};

// Forward-declared, deliberately not included: ModelImportData.h includes THIS
// header for SubMeshData, so including it back would be a cycle. A reference
// parameter needs nothing more than this.
namespace nous::engine::resource_manager { struct ModelImportData; }

struct ImporterMesh : IResourceImporter
{
    bool Import(const MetaFileData& metaFileData) override;

    // Writes the mesh half of an already-parsed model. Exists so ImportPipeline
    // can drive one ParseModel across the mesh, skeleton and animation importers
    // (spec step 6) instead of each of them re-opening the same FBX. Import()
    // above is currently the only caller, and parses on the spot.
    static bool SaveModel(const MetaFileData& metaFileData,
                          const nous::engine::resource_manager::ModelImportData& model);
    bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;
    void Evict(ResourceBase* resource) override;
    bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Loads EVERY submesh, geometry included. Only Deserialize should want this --
    // it merges them all into one ResourceMesh. Prefer the two below anywhere else;
    // this one is O(file size) by definition.
    static std::vector<SubMeshData> LoadHierarchy(const std::string& libraryPath);

    // Names, transforms and material paths only -- no vertices, no indices. On a V4
    // binary this reads the directory plus a few dozen bytes per submesh instead of
    // the whole file. Legacy binaries fall back to a full parse and discard the
    // geometry, so the call is always correct, just not always cheap.
    static std::vector<SubMeshInfo> LoadSubmeshInfo(const std::string& libraryPath);

    // One submesh by index. On V4 this seeks straight to its blob and reads only
    // that. Returns false if the index is out of range or the read fails.
    static bool LoadSubmesh(const std::string& libraryPath, int32_t submeshIndex,
                            SubMeshData& out);
};

#endif // IMPORTERMESH_H

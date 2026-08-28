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
// (empty for legacy V2 binaries or submeshes whose assimp material had no usable
// texture slots). SpawnMeshAsHierarchy resolves it to a ResourceMaterial.
struct SubMeshData
{
    std::string         name;
    glm::mat4           localTransform { 1.0f };
    std::vector<Vertex3D>  vertices;
    std::vector<uint32_t>  indices;
    std::string         materialAssetPath;
};

struct ImporterMesh : IResourceImporter
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;
    void Evict(ResourceBase* resource) override;
    bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Loads all submeshes from a library binary written by this importer.
    // Returns one SubMeshData per logical submesh; empty on failure.
    // Used by ModuleScene::SpawnMeshAsHierarchy to build per-submesh GameObjects.
    static std::vector<SubMeshData> LoadHierarchy(const std::string& libraryPath);
};

#endif // IMPORTERMESH_H

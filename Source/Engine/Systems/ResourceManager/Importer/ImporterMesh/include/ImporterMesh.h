#ifndef IMPORTERMESH_H
#define IMPORTERMESH_H

#include "Engine/Systems/ResourceManager/Importer/Importer.inl"
#include "Engine/Utils/Math/Vertex.inl"

#include <glm/glm.hpp>
#include <string>
#include <vector>

// One logical submesh extracted from an Assimp scene node.
// localTransform is the accumulated world transform from the scene root to this
// node (column-major, GLM convention). Vertices are in the submesh's local space.
struct SubMeshData
{
    std::string         name;
    glm::mat4           localTransform { 1.0f };
    std::vector<Vertex3D>  vertices;
    std::vector<uint32_t>  indices;
};

struct ImporterMesh : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Load(const std::string& libraryPath, Resource* outResource) override;
    bool Unload(Resource* inResource) override;

    // Loads all submeshes from a library binary written by this importer.
    // Returns one SubMeshData per logical submesh; empty on failure.
    // Used by ModuleScene::SpawnMeshAsHierarchy to build per-submesh GameObjects.
    static std::vector<SubMeshData> LoadHierarchy(const std::string& libraryPath);
};

#endif // IMPORTERMESH_H

#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Core/Resource/Resource.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

JsonObject CMesh::Serialize() const {
    JsonObject root;
    root.Set("type",      GetType());
    root.Set("assetPath", mesh ? mesh->GetAssetsPath() : "");

    // Store library path and UID so GameApp can load without .meta files.
    if (mesh)
    {
        root.Set("libraryPath", mesh->GetLibraryPath());
        root.Set("resourceUID", static_cast<double>(mesh->GetUID()));
    }

    // Only write submeshIndex for actual sub-resources; omit for merged meshes so
    // old scene files (without this field) continue to load via CreateResource().
    if (submeshIndex >= 0)
        root.Set("submeshIndex", submeshIndex);

    return root;
}

void CMesh::Deserialize(const JsonObject& obj) {
    const std::string assetPathStr = obj.GetString("assetPath");
    const std::string libraryPath  = obj.GetString("libraryPath");

    // Neither path set — GO has no mesh reference, leave it null.
    if (assetPathStr.empty() && libraryPath.empty())
    {
        mesh = nullptr;
        return;
    }
    const uint32 resourceUID = static_cast<uint32>(obj.GetDouble("resourceUID", 0.0));

    if (obj.HasKey("submeshIndex"))
    {
        submeshIndex = obj.GetInt("submeshIndex");

        auto go = GetGameObject();
        Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
        if (!scene) { mesh = nullptr; return; }

        if (!libraryPath.empty())
        {
            mesh = scene->GetResourceManager()->RequestOrCreateSubMeshResourceFromLibrary(
                libraryPath, submeshIndex, assetPathStr.c_str(), resourceUID);
        }
        if (!mesh && !assetPathStr.empty())
        {
            mesh = scene->GetResourceManager()->RequestOrCreateSubMeshResource(assetPathStr.c_str(), submeshIndex);
        }
    }
    else
    {
        submeshIndex = -1;

        auto go = GetGameObject();
        Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
        if (!scene) { mesh = nullptr; return; }

        // Try library path first (GAME mode / no .meta needed).
        if (!libraryPath.empty() && resourceUID != 0)
        {
            mesh = down_cast<ResourceMesh*>(scene->GetResourceManager()->CreateResourceFromLibrary(
                resourceUID, ResourceType::MESH, nous::engine::filesystem::GetFilename(assetPathStr),
                assetPathStr, libraryPath));
        }
        if (!mesh && !assetPathStr.empty())
        {
            mesh = down_cast<ResourceMesh*>(scene->GetResourceManager()->CreateResource(assetPathStr.c_str()));
        }
    }
}

void CMesh::OnDestroy() {
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    if (mesh && mesh->IsValid() && scene)
    {
        scene->GetResourceManager()->UnloadResource(mesh->GetUID());
    }
}

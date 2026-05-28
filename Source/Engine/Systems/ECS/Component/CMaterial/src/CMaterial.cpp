#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------
JsonObject CMaterial::Serialize() const {
    JsonObject root;
    root.Set("type", GetType());
    if (material) {
        root.Set("assetPath",   material->GetAssetsPath());
        root.Set("libraryPath", material->GetLibraryPath());
        root.Set("resourceUID", static_cast<double>(material->GetUID()));
    } else {
        root.Set("assetPath", "");
    }
    return root;
}

void CMaterial::Deserialize(const JsonObject& obj) {
    const std::string assetPath = obj.GetString("assetPath");
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;

    if (assetPath.empty()) {
        // No on-disk material was referenced — this GO used the in-memory default
        // material (no assetPath, UID=0). Restore it so the mesh still renders
        // after a snapshot round-trip, instead of leaving material=nullptr.
        if (scene)
            material = scene->GetResourceManager()->GetDefaultMaterial();
        return;
    }

    if (!scene) { material = nullptr; return; }

    const std::string libraryPath = obj.GetString("libraryPath");
    const uint32 resourceUID = static_cast<uint32>(obj.GetDouble("resourceUID", 0.0));

    // Try library path first (GAME mode / no .meta needed).
    if (!libraryPath.empty() && resourceUID != 0)
    {
        material = down_cast<ResourceMaterial*>(scene->GetResourceManager()->CreateResourceFromLibrary(
            resourceUID, ResourceType::MATERIAL, nous::engine::filesystem::GetFilename(assetPath),
            assetPath, libraryPath));
    }
    if (!material)
    {
        material = down_cast<ResourceMaterial*>(
            scene->GetResourceManager()->CreateResource(assetPath)
        );
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void CMaterial::OnDestroy()
{
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    if (material && material->IsValid() && scene)
    {
        scene->GetResourceManager()->UnloadResource(material->GetUID());
    }
}

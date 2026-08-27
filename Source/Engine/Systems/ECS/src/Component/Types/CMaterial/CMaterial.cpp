#include <ECS/Component/Types/CMaterial.h>

#include <ECS/ComponentServices.h>
#include <ECS/GameObject.h>
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/IResourceLoader.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/ResourceBase.h"
#include <FileSystem/FileSystem.h>

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

    // Null in a headless / test scene.
    IResourceLoader* rm = Services().resources;

    if (assetPath.empty()) {
        // No on-disk material was referenced — this GO used the in-memory default
        // material (no assetPath, UID=0). Restore it so the mesh still renders
        // after a snapshot round-trip, instead of leaving material=nullptr.
        if (rm)
            material = rm->GetDefaultMaterial();
        return;
    }

    if (!rm) { material = nullptr; return; }

    const std::string libraryPath = obj.GetString("libraryPath");
    const uint32 resourceUID = static_cast<uint32>(obj.GetDouble("resourceUID", 0.0));

    // Try library path first (GAME mode / no .meta needed).
    // NOTE: the null check before down_cast is load-bearing — down_cast asserts on
    // null (Globals.h), and a failed resolve (missing/deleted asset) legitimately
    // returns null. Same guarded shape as CAudioSource / CVideoPlayer.
    if (!libraryPath.empty() && resourceUID != 0)
    {
        if (ResourceBase* r = rm->CreateResourceFromLibrary(
                resourceUID, ResourceType::MATERIAL, nous::engine::filesystem::GetFilename(assetPath),
                assetPath, libraryPath))
            material = down_cast<ResourceMaterial*>(r);
    }
    if (!material)
    {
        if (ResourceBase* r = rm->CreateResource(assetPath))
            material = down_cast<ResourceMaterial*>(r);
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void CMaterial::OnDestroy()
{
    IResourceLoader* rm = Services().resources;
    if (material && material->IsLoaded() && rm)
    {
        rm->UnloadResource(material->GetUID());
    }
}

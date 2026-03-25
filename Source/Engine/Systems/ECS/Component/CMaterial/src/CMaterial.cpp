#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include <parson.h>

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------
JSON_Value* CMaterial::Serialize() const {
    JSON_Value* objVal = json_value_init_object();
    JSON_Object* obj = json_value_get_object(objVal);

    json_object_set_string(obj, "type", GetType().c_str());

    if (material) {
        json_object_set_string(obj, "assetPath", material->GetAssetsPath().c_str());
        json_object_set_string(obj, "libraryPath", material->GetLibraryPath().c_str());
        json_object_set_number(obj, "resourceUID", static_cast<double>(material->GetUID()));
    } else {
        json_object_set_string(obj, "assetPath", "");
    }

    return objVal;
}

void CMaterial::Deserialize(JSON_Object* obj) {
    const char* assetPath = json_object_get_string(obj, "assetPath");
    if (!assetPath || strlen(assetPath) == 0) {
        material = nullptr;
        return;
    }

    const char* libraryPathRaw = json_object_get_string(obj, "libraryPath");
    const std::string libraryPath = libraryPathRaw ? libraryPathRaw : "";
    const JSON_Value* uidVal      = json_object_get_value(obj, "resourceUID");
    const UID resourceUID         = uidVal ? static_cast<UID>(json_value_get_number(uidVal)) : 0;

    // Try library path first (GAME mode / no .meta needed).
    if (!libraryPath.empty() && resourceUID != 0)
    {
        material = down_cast<ResourceMaterial*>(External->resourceManager->CreateResourceFromLibrary(
            resourceUID, ResourceType::MATERIAL, NOUS_FileManager::GetFilename(assetPath),
            assetPath, libraryPath));
    }
    if (!material)
    {
        material = down_cast<ResourceMaterial*>(
            External->resourceManager->CreateResource(assetPath)
        );
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void CMaterial::OnDestroy()
{
    // Guard: External is set to nullptr in MainEditor.cpp before Application
    // destruction.  If we're called during shutdown after that point, the
    // ResourceManager is no longer reachable.
    if (External && material && material->IsValid()) {
        External->resourceManager->UnloadResource(material->GetUID());
    }
}

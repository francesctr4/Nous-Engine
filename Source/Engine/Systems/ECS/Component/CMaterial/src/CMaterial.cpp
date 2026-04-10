#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
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
        // No on-disk material was referenced — this GO used the in-memory default
        // material (no assetPath, UID=0). Restore it so the mesh still renders
        // after a snapshot round-trip, instead of leaving material=nullptr.
        material = m_GameObject->GetScene()->GetResourceManager()->GetDefaultMaterial();
        return;
    }

    const char* libraryPathRaw = json_object_get_string(obj, "libraryPath");
    const std::string libraryPath = libraryPathRaw ? libraryPathRaw : "";
    const JSON_Value* uidVal      = json_object_get_value(obj, "resourceUID");
    const uint32 resourceUID         = uidVal ? static_cast<uint32>(json_value_get_number(uidVal)) : 0;

    // Try library path first (GAME mode / no .meta needed).
    if (!libraryPath.empty() && resourceUID != 0)
    {
        material = down_cast<ResourceMaterial*>(m_GameObject->GetScene()->GetResourceManager()->CreateResourceFromLibrary(
            resourceUID, ResourceType::MATERIAL, NOUS_FileManager::GetFilename(assetPath),
            assetPath, libraryPath));
    }
    if (!material)
    {
        material = down_cast<ResourceMaterial*>(
            m_GameObject->GetScene()->GetResourceManager()->CreateResource(assetPath)
        );
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void CMaterial::OnDestroy()
{
    if (material && material->IsValid() && m_GameObject && m_GameObject->GetScene())
    {
        m_GameObject->GetScene()->GetResourceManager()->UnloadResource(material->GetUID());
    }
}

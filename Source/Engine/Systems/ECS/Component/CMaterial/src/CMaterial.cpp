#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"

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
    } else {
        json_object_set_string(obj, "assetPath", "");
    }

    return objVal;
}

void CMaterial::Deserialize(JSON_Object* obj) {
    const char* assetPath = json_object_get_string(obj, "assetPath");
    if (assetPath && strlen(assetPath) > 0) {
        material = down_cast<ResourceMaterial*>(
            External->resourceManager->CreateResource(assetPath)
        );
    } else {
        material = nullptr;
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

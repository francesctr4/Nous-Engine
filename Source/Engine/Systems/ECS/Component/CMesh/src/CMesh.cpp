#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Core/Application.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

// Parson
#include <parson.h>

JSON_Value *CMesh::Serialize() const {
    JSON_Value* objVal = json_value_init_object();
    JSON_Object* obj = json_value_get_object(objVal);

    json_object_set_string(obj, "type", GetType().c_str());

    if (mesh) {
        // Save the asset path to recreate the resource later
        json_object_set_string(obj, "assetPath", mesh->GetAssetsPath().c_str());
    } else {
        json_object_set_string(obj, "assetPath", "");
    }

    return objVal;
}

void CMesh::Deserialize(JSON_Object *obj) {
    const char* assetPath = json_object_get_string(obj, "assetPath");
    if (assetPath && strlen(assetPath) > 0) {
        // Create or load the resource via the resource manager
        mesh = down_cast<ResourceMesh*>(
                External->resourceManager->CreateResource(assetPath)
        );
    } else {
        mesh = nullptr;
    }
}

void CMesh::OnDestroy() {
    if (mesh && mesh->IsValid())
    {
        External->resourceManager->UnloadResource(mesh->GetUID());
    }
}

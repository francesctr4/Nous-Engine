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
    json_object_set_string(obj, "assetPath", mesh ? mesh->GetAssetsPath().c_str() : "");

    // Only write submeshIndex for actual sub-resources; omit for merged meshes so
    // old scene files (without this field) continue to load via CreateResource().
    if (submeshIndex >= 0)
        json_object_set_number(obj, "submeshIndex", static_cast<double>(submeshIndex));

    return objVal;
}

void CMesh::Deserialize(JSON_Object *obj) {
    const char* assetPath = json_object_get_string(obj, "assetPath");
    if (!assetPath || strlen(assetPath) == 0)
    {
        mesh = nullptr;
        return;
    }

    // Detect whether this component references a specific submesh.
    // json_object_get_value returns nullptr when the key is absent (old scenes).
    const JSON_Value* subIdxVal = json_object_get_value(obj, "submeshIndex");
    if (subIdxVal)
    {
        submeshIndex = static_cast<int32_t>(json_value_get_number(subIdxVal));
        mesh = External->resourceManager->RequestOrCreateSubMeshResource(assetPath, submeshIndex);
    }
    else
    {
        submeshIndex = -1;
        mesh = down_cast<ResourceMesh*>(External->resourceManager->CreateResource(assetPath));
    }
}

void CMesh::OnDestroy() {
    // Guard: External is set to nullptr in MainEditor.cpp before Application
    // destruction.  If we're called during shutdown after that point, the
    // ResourceManager is no longer reachable.
    if (External && mesh && mesh->IsValid())
    {
        External->resourceManager->UnloadResource(mesh->GetUID());
    }
}

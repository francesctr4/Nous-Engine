#ifndef NOUS_ENGINE_COMPONENTMESH_H
#define NOUS_ENGINE_COMPONENTMESH_H

#include "Systems/Resource Manager/Resource Types/ResourceMesh.h"
#include "Modules/ModuleResourceManager.h"

class CMesh : public Component {
public:
    COMPONENT_TYPE(CMesh)

    ResourceMesh* mesh;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override
    {
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

    void Deserialize(JSON_Object* obj) override
    {
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

    void OnDestroy() override
    {
        if (mesh->IsValid())
        {
            External->resourceManager->UnloadResource(mesh->GetUID());
        }
    }
};

#endif // NOUS_ENGINE_COMPONENTMESH_H

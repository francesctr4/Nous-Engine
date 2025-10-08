//
// Created by TheFr on 03/10/2025.
//

#ifndef NOUS_ENGINE_COMPONENTMESH_H
#define NOUS_ENGINE_COMPONENTMESH_H

#include "Systems/Resource Manager/Resource Types/ResourceMesh.h"
#include "Modules/ModuleResourceManager.h"

class CMesh : public Component {
public:
    COMPONENT_TYPE(CMesh)
    ResourceMesh* mesh = nullptr;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override
    {
        return nullptr;
    }

    void Deserialize(JSON_Object* obj) override
    {

    }

    ~CMesh() {
        //External->resourceManager->UnloadResource(mesh->GetUID());
    }
};


#endif //NOUS_ENGINE_COMPONENTMESH_H

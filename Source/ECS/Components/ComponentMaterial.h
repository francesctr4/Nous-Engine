//
// Created by TheFr on 03/10/2025.
//

#ifndef NOUS_ENGINE_COMPONENTMATERIAL_H
#define NOUS_ENGINE_COMPONENTMATERIAL_H

#include "Systems/Resource Manager/Resource Types/ResourceMaterial.h"

class CMaterial : public Component {
public:
    COMPONENT_TYPE(CMaterial)
    ResourceMaterial* material = nullptr;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override
    {
        return nullptr;
    }

    void Deserialize(JSON_Object* obj) override
    {

    }

    ~CMaterial() {
        //External->resourceManager->UnloadResource(material->GetUID());
    }
};

#endif //NOUS_ENGINE_COMPONENTMATERIAL_H

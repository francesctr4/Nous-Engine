//
// Created by TheFr on 03/10/2025.
//

#ifndef NOUS_ENGINE_COMPONENTMATERIAL_H
#define NOUS_ENGINE_COMPONENTMATERIAL_H

#include <Engine/Systems/Resource Manager/Resource Types/ResourceMaterial.h>
#include <Engine/Core/Modules/ModuleResourceManager.h>
#include <Engine/Core/Application.h>

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

    void OnDestroy() override
    {
        if (material->IsValid())
        {
            External->resourceManager->UnloadResource(material->GetUID());
        }
    }
};

#endif //NOUS_ENGINE_COMPONENTMATERIAL_H

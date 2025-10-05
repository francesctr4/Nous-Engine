//
// Created by TheFr on 03/10/2025.
//

#ifndef NOUS_ENGINE_COMPONENTMATERIAL_H
#define NOUS_ENGINE_COMPONENTMATERIAL_H

#include "Systems/Resource Manager/Resource Types/ResourceMaterial.h"

struct CMaterial : public Component {
    COMPONENT_TYPE(CMaterial)
    ResourceMaterial* material = nullptr;

    ~CMaterial() {
        //External->resourceManager->UnloadResource(material->GetUID());
    }
};

#endif //NOUS_ENGINE_COMPONENTMATERIAL_H

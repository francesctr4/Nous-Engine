#ifndef NOUS_ENGINE_CMATERIAL_H
#define NOUS_ENGINE_CMATERIAL_H

#include "Engine/Systems/ECS/Component/Component.h"

class ResourceMaterial;

class CMaterial : public Component {
public:
    COMPONENT_TYPE(CMaterial)

    CMaterial() = default;
    ~CMaterial() override = default;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override;
    void Deserialize(JSON_Object* obj) override;

    void OnDestroy() override;

    ResourceMaterial* material = nullptr;
};

#endif // NOUS_ENGINE_CMATERIAL_H

#ifndef NOUS_ENGINE_CMATERIAL_H
#define NOUS_ENGINE_CMATERIAL_H

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

class ResourceMaterial;

class CMaterial : public Component {
public:
    COMPONENT_TYPE(CMaterial)

    CMaterial() = default;
    ~CMaterial() override = default;

    // ---------- JSON Serialization ----------
    NOUS_ENGINE_API JSON_Value* Serialize() const override;
    NOUS_ENGINE_API void Deserialize(JSON_Object* obj) override;

    NOUS_ENGINE_API void OnDestroy() override;

    ResourceMaterial* material = nullptr;
};

#endif // NOUS_ENGINE_CMATERIAL_H

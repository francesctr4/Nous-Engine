#pragma once

#include <ECS/Component/Component.h>
#include "Engine/EngineExport.h"

class ResourceMaterial;

class CMaterial : public Component {
public:
    COMPONENT_TYPE(CMaterial)

    // ---------- JSON Serialization ----------
    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void Deserialize(const JsonObject& obj) override;

    NOUS_ENGINE_API void OnDestroy() override;

    ResourceMaterial* material = nullptr;
};

#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

class CAudioSource : public Component {
public:
    COMPONENT_TYPE(CAudioSource)

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;

    // Lifecycle
    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;

private:



};


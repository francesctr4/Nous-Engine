#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/CAudioSource.h"


JsonObject CAudioSource::Serialize() const
{
    return JsonObject();
}

void CAudioSource::Deserialize(const JsonObject& obj)
{
}

void CAudioSource::OnUpdate(float deltaTime)
{
    Component::OnUpdate(deltaTime);
}

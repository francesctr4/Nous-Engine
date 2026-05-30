#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

GameObject Component::GetGameObject() const {
    return GameObject(m_entity, m_registry);
}

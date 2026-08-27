#include <ECS/Component/Component.h>
#include <ECS/GameObject.h>
#include <ECS/ComponentServices.h>

#include <entt/entt.hpp>

GameObject Component::GetGameObject() const {
    return GameObject(m_entity, m_registry);
}

const ComponentServices& Component::Services() const {
    static const ComponentServices s_empty;

    if (!m_registry) return s_empty;

    // find<> rather than get<>: get<> throws when the key is absent, and a bare
    // registry (a future test, or a Scene built before the ctx emplacement) must
    // degrade to the empty aggregate instead.
    const auto* found = m_registry->ctx().find<const ComponentServices*>();
    return (found && *found) ? **found : s_empty;
}

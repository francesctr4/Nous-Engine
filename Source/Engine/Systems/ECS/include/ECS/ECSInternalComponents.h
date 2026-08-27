#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <cstdint>

// Internal structural components used by Scene and GameObject.
// These are NOT user-facing components — they are never serialized as component
// entries. Their data surfaces through GameObject::GetID(), GetName(), GetParent(), etc.

struct CEntityInfo {
    uint32_t    id   = 0;
    std::string name;
};

struct CHierarchy {
    entt::entity            parent = entt::null;
    std::vector<entt::entity> children;
};

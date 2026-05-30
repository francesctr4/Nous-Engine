#pragma once

#include <string>
#include <string_view>
#include <entt/entity/fwd.hpp>     // entt::entity + entt::registry forward decls (full <entt/entt.hpp> not needed here)
#include <entt/entity/entity.hpp>  // entt::null
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

class GameObject;

class Component {
public:
    virtual ~Component() = default;

    // Returns a lightweight GameObject handle reconstructed from the stored entity.
    // Valid as long as the entity exists in the registry.
    GameObject GetGameObject() const;

    virtual std::string_view GetType() const = 0;

    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    virtual JsonObject Serialize()                    const = 0;
    virtual void       Deserialize(const JsonObject& obj)  = 0;

protected:
    friend class GameObject;
    entt::entity    m_entity   = entt::null;
    entt::registry* m_registry = nullptr;
};

#define COMPONENT_TYPE(type) \
    static constexpr std::string_view TypeName = #type; \
    std::string_view GetType() const override { return TypeName; }

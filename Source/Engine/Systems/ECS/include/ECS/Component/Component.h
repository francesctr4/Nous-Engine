#pragma once

#include <string>
#include <string_view>
#include <entt/entity/fwd.hpp>     // entt::entity + entt::registry forward decls (full <entt/entt.hpp> not needed here)
#include <entt/entity/entity.hpp>  // entt::null
#include <Utils/Serialization/JsonObject.h>

class GameObject;
struct ComponentServices;

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

    // The engine services this component may use — the seam that keeps Systems/
    // from depending on Modules/. Never null; individual FIELDS may be null in a
    // headless scene, so guard the service you use, not this.
    //
    // Resolved from the registry context rather than stored per component: a
    // stored pointer would cost 8 bytes on every component including CTransform
    // (thousands of instances) to save a lookup only ~6 component types perform.
    // Resolve once per hook into a local.
    const ComponentServices& Services() const;

    entt::entity    m_entity   = entt::null;
    entt::registry* m_registry = nullptr;
};

#define COMPONENT_TYPE(type) \
    static constexpr std::string_view TypeName = #type; \
    std::string_view GetType() const override { return TypeName; }

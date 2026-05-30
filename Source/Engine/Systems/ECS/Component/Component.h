#pragma once

#include <string>
#include <string_view>
#include <entt/entt.hpp>
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

class GameObject;

class Component {
public:
    virtual ~Component() = default;

    // Returns a lightweight GameObject handle reconstructed from the stored entity.
    // Valid as long as the entity exists in the registry.
    GameObject GetGameObject() const;

    virtual std::string GetType() const = 0;

    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    virtual JsonObject Serialize()                    const = 0;
    virtual void       Deserialize(const JsonObject& obj)  = 0;

protected:
    friend class GameObject;
    entt::entity    m_Entity   = entt::null;
    entt::registry* m_Registry = nullptr;
};

#define COMPONENT_TYPE(type) \
    static constexpr std::string_view TypeName = #type; \
    std::string GetType() const override { return std::string(TypeName); }

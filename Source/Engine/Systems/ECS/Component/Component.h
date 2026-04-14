#pragma once

#include <string>
#include <entt/entt.hpp>

class GameObject;

typedef struct json_object_t JSON_Object;
typedef struct json_array_t  JSON_Array;
typedef struct json_value_t  JSON_Value;

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

    virtual JSON_Value* Serialize()          const = 0;
    virtual void        Deserialize(JSON_Object* obj) = 0;

    static Component* CreateComponent(const std::string& type);

protected:
    friend class GameObject;
    entt::entity    m_Entity   = entt::null;
    entt::registry* m_Registry = nullptr;
};

#define COMPONENT_TYPE(type) \
    std::string GetType() const override { return #type; }

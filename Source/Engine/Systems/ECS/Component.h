#ifndef NOUS_ENGINE_COMPONENT_H
#define NOUS_ENGINE_COMPONENT_H

#include <string>
#include <memory>

// Forward declaration
class GameObject;

typedef struct json_object_t JSON_Object;
typedef struct json_array_t  JSON_Array;
typedef struct json_value_t  JSON_Value;

class Component {
public:
    virtual ~Component() = default;

    // Getters
    GameObject* GetGameObject() const { return m_GameObject; }
    virtual std::string GetType() const = 0;

    // Lifecycle methods
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    // Serialization
    virtual JSON_Value* Serialize() const = 0;
    virtual void Deserialize(JSON_Object* obj) = 0;

    // Static method for component creation during deserialization
    static std::unique_ptr<Component> CreateComponent(const std::string& type);

protected:
    friend class GameObject;
    GameObject* m_GameObject = nullptr;
};

// Macro to easily implement component type
#define COMPONENT_TYPE(type) \
    std::string GetType() const override { return #type; }

#endif //NOUS_ENGINE_COMPONENT_H

#ifndef NOUS_ENGINE_COMPONENT_H
#define NOUS_ENGINE_COMPONENT_H

#include <string>

// Forward declaration
class GameObject;

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
    virtual void Serialize() {}
    virtual void Deserialize() {}

protected:
    friend class GameObject;
    GameObject* m_GameObject = nullptr;
};

// Macro to easily implement component type
#define COMPONENT_TYPE(type) \
    std::string GetType() const override { return #type; }

#endif //NOUS_ENGINE_COMPONENT_H

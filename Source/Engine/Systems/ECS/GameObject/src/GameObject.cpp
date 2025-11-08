#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Core/Logging System/Logger.h"

#include <parson.h>

// -----------------------------------------------------------------------------
// Constructors / Destructor
// -----------------------------------------------------------------------------
GameObject::GameObject() = default;

GameObject::GameObject(uint32_t id, const std::string& name)
        : m_ID(id), m_Name(name) {}

GameObject::~GameObject() {
    for (auto& [type, component] : m_Components)
        component->OnDestroy();

    m_Components.clear();
    ClearChildren();
}

// -----------------------------------------------------------------------------
// Component Management (non-template)
// -----------------------------------------------------------------------------
void GameObject::AddComponent(std::unique_ptr<Component> component) {
    if (!component) return;

    component->m_GameObject = this;
    auto type = std::type_index(typeid(*component));
    Component* ptr = component.get();

    m_Components[type] = std::move(component);
    ptr->OnStart();
}

void GameObject::UpdateComponents(float deltaTime) {
    for (auto& [type, component] : m_Components)
        component->OnUpdate(deltaTime);
}

std::vector<Component*> GameObject::GetAllComponents() {
    std::vector<Component*> components;
    for (auto& [type, component] : m_Components)
        components.push_back(component.get());
    return components;
}

// -----------------------------------------------------------------------------
// Info
// -----------------------------------------------------------------------------
uint32_t GameObject::GetID() const { return m_ID; }
void GameObject::SetName(const std::string& name) { m_Name = name; }
const std::string& GameObject::GetName() const { return m_Name; }

// -----------------------------------------------------------------------------
// Hierarchy
// -----------------------------------------------------------------------------
GameObject* GameObject::GetParent() const { return m_Parent; }
const std::vector<GameObject*>& GameObject::GetChildren() const { return m_Children; }

void GameObject::SetParent(GameObject* parent) {
    if (m_Parent == parent) return;

    if (m_Parent) {
        auto& siblings = m_Parent->m_Children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    m_Parent = parent;

    if (m_Parent)
        m_Parent->m_Children.push_back(this);
}

void GameObject::AddChild(GameObject* child) {
    if (child && child != this)
        child->SetParent(this);
}

void GameObject::RemoveChild(GameObject* child) {
    if (child) {
        auto it = std::find(m_Children.begin(), m_Children.end(), child);
        if (it != m_Children.end()) {
            m_Children.erase(it);
            child->m_Parent = nullptr;
        }
    }
}

void GameObject::ClearChildren() {
    auto childrenCopy = m_Children;
    for (auto* child : childrenCopy)
        RemoveChild(child);
}

GameObject* GameObject::FindChildByName(const std::string& name, bool recursive) {
    for (auto* child : m_Children) {
        if (child->GetName() == name)
            return child;

        if (recursive) {
            GameObject* found = child->FindChildByName(name, true);
            if (found) return found;
        }
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------
JSON_Value* GameObject::Serialize() const {
    JSON_Value* objVal = json_value_init_object();
    JSON_Object* obj = json_value_get_object(objVal);

    json_object_set_number(obj, "uid", m_ID);
    json_object_set_string(obj, "name", m_Name.c_str());
    uint32_t parentID = m_Parent ? m_Parent->GetID() : 0;
    json_object_set_number(obj, "parent", parentID);

    NOUS_INFO("Serializing: %s (ID: %u) -> Parent ID: %u", m_Name.c_str(), m_ID, parentID);

    JSON_Value* componentsVal = json_value_init_array();
    JSON_Array* componentsArr = json_value_get_array(componentsVal);

    for (const auto& [type, component] : m_Components)
        json_array_append_value(componentsArr, component->Serialize());

    json_object_set_value(obj, "components", componentsVal);
    return objVal;
}

std::unique_ptr<GameObject> GameObject::Deserialize(JSON_Object* obj) {
    uint32_t uid = static_cast<uint32_t>(json_object_get_number(obj, "uid"));
    const char* name = json_object_get_string(obj, "name");
    uint32_t parentID = static_cast<uint32_t>(json_object_get_number(obj, "parent"));

    auto go = std::make_unique<GameObject>(uid, name ? name : "");
    go->m_ParentID = parentID;

    NOUS_INFO("Deserializing: %s (ID: %u) -> Parent ID: %u", name ? name : "", uid, parentID);

    JSON_Array* comps = json_object_get_array(obj, "components");
    if (comps) {
        size_t count = json_array_get_count(comps);
        for (size_t i = 0; i < count; ++i) {
            JSON_Object* compObj = json_array_get_object(comps, i);
            const char* type = json_object_get_string(compObj, "type");

            if (type) {
                auto component = Component::CreateComponent(type);
                if (component) {
                    component->Deserialize(compObj);
                    go->AddComponent(std::move(component));
                }
            }
        }
    }
    return go;
}

// -----------------------------------------------------------------------------
// Parent ID
// -----------------------------------------------------------------------------
uint32_t GameObject::GetParentID() const { return m_ParentID; }

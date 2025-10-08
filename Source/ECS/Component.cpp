#include "ECS/Component.h"
#include "ECS/Components/ComponentTransform.h"

std::unique_ptr<Component> Component::CreateComponent(const std::string& type) {
    if (type == "CTransform") {
        return std::make_unique<CTransform>();
    }

    // Add other component types here as you create them
    // if (type == "CRenderer") return std::make_unique<CRenderer>();
    // if (type == "CScript") return std::make_unique<CScript>();

    printf("Unknown component type: %s\n", type.c_str());
    return nullptr;
}

#include "Component.h"
#include "Engine/Systems/ECS/Components/ComponentTransform.h"
#include "Engine/Systems/ECS/Components/ComponentMaterial.h"
#include "Engine/Systems/ECS/Components/ComponentMesh.h"

#include "Engine/Core/Logging System/Logger.h"

std::unique_ptr<Component> Component::CreateComponent(const std::string& type) {
    if (type == "CTransform") {
        return std::make_unique<CTransform>();
    }

    if (type == "CMesh") {
        return std::make_unique<CMesh>();
    }

    if (type == "CMaterial") {
        return std::make_unique<CMaterial>();
    }

    NOUS_WARN("[%s] Unable to create component. "
              "Unknown component type: %s.", type.c_str());

    return nullptr;
}

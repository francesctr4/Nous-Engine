#include <Engine/ECS/Component.h>
#include <Engine/ECS/Components/ComponentTransform.h>
#include <Engine/ECS/Components/ComponentMaterial.h>
#include <Engine/ECS/Components/ComponentMesh.h>

#include <Engine/Utils/Logger.h>

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
              "Unknown component type: %s. \n", type.c_str());

    return nullptr;
}

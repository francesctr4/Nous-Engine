#include "Component.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"

#include "Engine/Core/Logger/Logger.h"

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

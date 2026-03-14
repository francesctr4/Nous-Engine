#include "Component.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

Component* Component::CreateComponent(const std::string& type) {
    if (type == "CTransform") {
        return NOUS_NEW<CTransform>(MemoryTag::COMPONENT);
    }

    if (type == "CMesh") {
        return NOUS_NEW<CMesh>(MemoryTag::COMPONENT);
    }

    if (type == "CMaterial") {
        return NOUS_NEW<CMaterial>(MemoryTag::COMPONENT);
    }

    if (type == "CCamera") {
        return NOUS_NEW<CCamera>(MemoryTag::COMPONENT);
    }

    NOUS_WARN("[%s] Unable to create component. "
              "Unknown component type: %s.", type.c_str());

    return nullptr;
}

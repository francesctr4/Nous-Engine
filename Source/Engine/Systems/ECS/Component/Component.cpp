#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"

GameObject Component::GetGameObject() const {
    return GameObject(m_Entity, m_Registry);
}

Component* Component::CreateComponent(const std::string& type) {
    if (type == "CTransform") return new CTransform();
    if (type == "CMesh")      return new CMesh();
    if (type == "CMaterial")  return new CMaterial();
    if (type == "CCamera")    return new CCamera();
    if (type == "CLight")     return new CLight();
    if (type == "CScript")    return new CScript();
    if (type == "CPrefab")    return new CPrefab();
    return nullptr;
}

#pragma once

#include "Engine/Systems/ECS/Component/ComponentList.h"

#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/Types/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/Types/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/Types/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/Types/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/Types/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/Types/CPrefab/include/CPrefab.h"

// ─────────────────────────────────────────────────────────────────────────────
// THE single edit site for registering an ECS component type.
// Add the new type here (and its include above). Declaration order is the
// canonical order for serialization, enumeration, and OnDestroy teardown.
// ─────────────────────────────────────────────────────────────────────────────
using ComponentTypes = ComponentList<
    CTransform,
    CMesh,
    CMaterial,
    CCamera,
    CLight,
    CScript,
    CPrefab
>;

#pragma once

#include "Engine/Systems/ECS/Component/ComponentList.h"

#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"

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

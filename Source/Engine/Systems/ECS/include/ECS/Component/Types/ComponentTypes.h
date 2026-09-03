#pragma once


#include <ECS/Component/ComponentList.h>

#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CMesh/CMesh.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <ECS/Component/Types/CLight/CLight.h>
#include <ECS/Component/Types/CScript/CScript.h>
#include <ECS/Component/Types/CPrefab/CPrefab.h>
#include <ECS/Component/Types/CAudioSource/CAudioSource.h>
#include <ECS/Component/Types/CAudioListener/CAudioListener.h>
#include <ECS/Component/Types/CVideoPlayer/CVideoPlayer.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>

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
    CPrefab,
    CAudioSource,
    CAudioListener,
    CVideoPlayer,
    CAnimator
>;

// Subset of ComponentTypes whose OnUpdate does real per-frame work. Scene::Update
// ticks exactly these — driving the full ComponentTypes there would needlessly walk
// the CTransform/CMesh/CMaterial/CPrefab views every frame to call no-op OnUpdates.
// Keep in sync when a component gains (or loses) a meaningful OnUpdate override.
using UpdatableComponentTypes = ComponentList<
    CScript,
    CCamera,
    CLight,
    CAudioSource,
    CAudioListener,
    CVideoPlayer,
    CAnimator
>;

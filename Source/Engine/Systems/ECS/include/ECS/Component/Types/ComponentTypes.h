#pragma once


#include <ECS/Component/ComponentList.h>

#include <ECS/Component/Types/CTransform.h>
#include <ECS/Component/Types/CMesh.h>
#include <ECS/Component/Types/CMaterial.h>
#include <ECS/Component/Types/CCamera.h>
#include <ECS/Component/Types/CLight.h>
#include <ECS/Component/Types/CScript.h>
#include <ECS/Component/Types/CPrefab.h>
#include <ECS/Component/Types/CAudioSource.h>
#include <ECS/Component/Types/CAudioListener.h>
#include <ECS/Component/Types/CVideoPlayer.h>

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
    CVideoPlayer
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
    CVideoPlayer
>;

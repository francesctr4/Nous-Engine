#pragma once

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#include <entt/entt.hpp>

class Camera;

// Per-frame snapshot of scene state required by the renderer.
// Produced by ModuleScene::PostUpdate; consumed by ModuleRenderer3D::PostUpdate.
struct SceneRenderData
{
    bool            hasActiveScene = false;
    Camera*         gameCamera     = nullptr;
    GameObject      selectedObject;           // null handle if nothing selected
    entt::registry* registry       = nullptr; // non-owning; valid for this frame only
};

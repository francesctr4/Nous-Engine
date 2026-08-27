#pragma once

#include <ECS/GameObject.h>

#include <entt/entt.hpp>
#include <vector>

class Camera;

// Per-frame snapshot of scene state required by the renderer.
// Produced by ModuleScene::PostUpdate; consumed by ModuleRenderer3D::PostUpdate.
struct SceneRenderData
{
    bool            hasActiveScene = false;
    Camera*         gameCamera     = nullptr;
    std::vector<GameObject> selectedObjects;  // empty if nothing selected
    GameObject              primaryObject;    // invalid handle if nothing selected
    entt::registry* registry       = nullptr; // non-owning; valid for this frame only
};

#pragma once

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#include <vector>

class Camera;

// Per-frame snapshot of scene state required by the renderer.
// Produced by ModuleScene::PostUpdate; consumed by ModuleRenderer3D::PostUpdate.
// GameObject handles are value-type (cheap to copy, valid for the frame they were captured).
struct SceneRenderData
{
    bool                    hasActiveScene = false;
    Camera*                 gameCamera     = nullptr;
    GameObject              selectedObject;           // null handle if nothing selected
    std::vector<GameObject> gameObjects;              // thread-safe snapshot for this frame
};

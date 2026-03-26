#pragma once

#include "Engine/Utils/DataStructures/NOUS_Vector.h"

class Camera;
class GameObject;

// Per-frame snapshot of scene state required by the renderer.
// Produced by ModuleScene::PostUpdate; consumed by ModuleRenderer3D::PostUpdate.
// Raw pointers are valid only for the frame they were captured — do not cache.
struct SceneRenderData
{
    bool                     hasActiveScene = false;
    Camera*                  gameCamera     = nullptr;
    GameObject*              selectedObject = nullptr; // editor-only selection, may be nullptr
    NOUS_Vector<GameObject*> gameObjects;              // thread-safe snapshot for this frame

    SceneRenderData() : gameObjects(MemoryTag::GAMEOBJECT) {}
};

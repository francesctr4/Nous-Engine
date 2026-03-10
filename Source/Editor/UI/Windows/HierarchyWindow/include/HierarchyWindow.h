#ifndef NOUS_ENGINE_HIERARCHYWINDOW_H
#define NOUS_ENGINE_HIERARCHYWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

class Scene;
class GameObject;

struct ReparentRequest {
    GameObject* child;
    GameObject* newParent;
};

class HierarchyWindow : public IEditorWindow
{
public:

    explicit HierarchyWindow(const char* title, EditorContext* context, bool start_open = true);

    void Init() override;
    void Draw() override;

    void SetScene(Scene* scene) { m_Scene = scene; }

private:
    void DrawGameObjectNode(GameObject* obj);

    Scene* m_Scene = nullptr;
    NOUS_Vector<GameObject*> m_ToDelete; // objects pending deletion
    NOUS_Vector<ReparentRequest> m_ToReparent;
    bool IsChildOf(GameObject *parent, GameObject *child);
};

#endif

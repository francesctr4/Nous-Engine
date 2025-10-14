#ifndef NOUS_ENGINE_HIERARCHYWINDOW_H
#define NOUS_ENGINE_HIERARCHYWINDOW_H

#include <Editor/IEditorWindow.inl>

class Scene;
class GameObject;

struct ReparentRequest {
    GameObject* child;
    GameObject* newParent;
};

class HierarchyWindow : public IEditorWindow
{
public:

    explicit HierarchyWindow(const char* title, bool start_open = true);

    void Init() override;
    void Draw() override;

    void SetScene(Scene* scene) { m_Scene = scene; }
    GameObject* GetSelected() const { return m_Selected; }

private:
    void DrawGameObjectNode(GameObject* obj);

    Scene* m_Scene = nullptr;
    GameObject* m_Selected = nullptr;
    std::vector<GameObject*> m_ToDelete; // objects pending deletion
    std::vector<ReparentRequest> m_ToReparent;
    bool IsChildOf(GameObject *parent, GameObject *child);
};

#endif

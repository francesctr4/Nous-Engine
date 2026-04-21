#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#include <vector>

class Scene;

struct ReparentRequest {
    GameObject child;
    GameObject newParent;
};

class HierarchyWindow : public IEditorWindow
{
public:

    explicit HierarchyWindow(const char* title, EditorContext* context, bool start_open = true);

    void Init() override;
    void DrawContent() override;
    void FinishUpdate() override;

    void SetScene(Scene* scene) { m_Scene = scene; }

private:
    void DrawGameObjectNode(GameObject obj, bool insidePrefab = false);
    void DrawSaveAsPrefabPopup();

    Scene* m_Scene = nullptr;
    std::vector<GameObject> m_ToDelete;
    std::vector<ReparentRequest> m_ToReparent;
    static bool IsChildOf(GameObject parent, GameObject child);

    // Save-as-prefab popup state
    bool        m_showSaveAsPrefabPopup = false;
    char        m_prefabNameBuffer[128] = {};
    GameObject  m_prefabSaveTarget;
};

#pragma once

#include "Editor/UI/IEditorWindow.h"
#include <ECS/GameObject.h>

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

private:

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    void SetScene(Scene* scene);

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    bool DrawSceneRootNode() const;

    void HandleHierarchyDragDropPayloads();

    void HandleSceneContextMenu();

    void HandleEmptyClickSelection() const;

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    void DrawRootGameObjects();

    void DrawGameObjectNode(GameObject obj, bool insidePrefab = false);

    ImGuiTreeNodeFlags BuildNodeFlags(GameObject obj) const;

    bool ShouldApplyPrefabTint(GameObject obj, bool insidePrefab) const;

    void PushPrefabStyle(bool applyTint) const;

    bool DrawNode(GameObject obj, ImGuiTreeNodeFlags flags) const;

    void PopPrefabStyle(bool applyTint) const;

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    void HandleGameObjectSelection(GameObject obj) const;

    void HandleGameObjectNodeContextMenu(GameObject obj);

    void CreateGameObjectNodeDragDropSource(GameObject obj) const;

    void HandleGameObjectNodeDragDropPayloads(GameObject obj);

    static bool IsChildOf(GameObject parent, GameObject child);

    void DrawChildrenNodes(GameObject obj, bool applyTint);

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    void ProcessPendingGameObjectDeletion();

    void ProcessReparentingRequests();

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    void DrawSaveAsPrefabPopup();

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    Scene* m_Scene = nullptr;
    std::vector<GameObject> m_ToDelete;
    std::vector<ReparentRequest> m_ToReparent;

    // Save-as-prefab popup state
    bool        m_showSaveAsPrefabPopup = false;
    char        m_prefabNameBuffer[128] = {};
    GameObject  m_prefabSaveTarget;
};

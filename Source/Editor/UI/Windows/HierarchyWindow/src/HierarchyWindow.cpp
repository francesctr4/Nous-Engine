#include "Editor/UI/Windows/HierarchyWindow/include/HierarchyWindow.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "imgui.h"

HierarchyWindow::HierarchyWindow(const char* title, EditorContext* context, bool start_open)
        : IEditorWindow(title, context, nullptr, start_open),
        m_ToDelete(MemoryTag::SCENE), m_ToReparent(MemoryTag::SCENE)
{
    Init();
}

void HierarchyWindow::Init()
{
    SetScene(External->scene->activeScene);
}

void HierarchyWindow::Draw() {
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            if (m_Scene)
            {
                // Root node = Scene itself
                bool opened = ImGui::TreeNodeEx((void*)m_Scene,
                                                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow,
                                                m_Scene->GetName().c_str());

                // 🔹 Make the SCENE NODE a drag-drop target for reparenting to root
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
                        IM_ASSERT(payload->DataSize == sizeof(GameObject*));
                        GameObject* draggedGO = *(GameObject**)payload->Data;

                        // Reparent to root (nullptr)
                        m_ToReparent.push_back({ draggedGO, nullptr });
                    }
                    ImGui::EndDragDropTarget();
                }

                // Right-click on the scene root to create objects.
                if (ImGui::BeginPopupContextItem("##SceneContextMenu")) {
                    if (ImGui::MenuItem("Create Empty")) {
                        GameObject* go = m_Scene->CreateGameObject("GameObject", nullptr);
                        External->scene->selectedGameObject = go;
                    }
                    if (ImGui::MenuItem("Create Camera")) {
                        GameObject* go = m_Scene->CreateGameObject("Main Camera", nullptr);
                        auto& cam = go->AddComponent<CCamera>();
                        cam.isMainCamera = true;
                        External->scene->selectedGameObject = go;
                    }
                    ImGui::EndPopup();
                }

                if (opened) {
                    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
                        if (!ImGui::IsAnyItemHovered()) {
                            External->scene->selectedGameObject = nullptr;
                        }
                    }

                    // Snapshot under mutex — guards against concurrent CreateGameObject() calls
                    // from the background LoadScene job reallocating the vector mid-iteration.
                    const auto gameObjects = m_Scene->GetGameObjectsSnapshot();
                    for (auto& goPtr : gameObjects)
                    {
                        if (!goPtr->GetParent())
                        {
                            DrawGameObjectNode(goPtr);
                        }
                    }
                    ImGui::TreePop();
                }

                // Process deletion first
                for (auto* go : m_ToDelete) {
                    if (External->scene->selectedGameObject == go) {
                        External->scene->selectedGameObject = nullptr;
                    }
                    m_Scene->DestroyGameObject(go);
                }
                m_ToDelete.clear();

                // Process reparenting safely
                for (auto& req : m_ToReparent) {
                    req.child->SetParent(req.newParent);
                }
                m_ToReparent.clear();
            }
        }
        ImGui::End();
    }
}

void HierarchyWindow::DrawGameObjectNode(GameObject* go) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;

    if (go->GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Highlight if selected
    if (go == External->scene->selectedGameObject)
        flags |= ImGuiTreeNodeFlags_Selected;

    std::string label = go->GetName() + "###" + std::to_string(go->GetID());
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    // Left-click to select
    if (ImGui::IsItemClicked()) {
        // Optionally notify the application/inspector:
        External->scene->selectedGameObject = go;
    }

    // Right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            m_ToDelete.push_back(go);
            if (External->scene->selectedGameObject == go)
                External->scene->selectedGameObject = nullptr; // clear selection if deleted
        }
        ImGui::EndPopup();
    }

    // 🔹 Drag source
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("DND_GAMEOBJECT", &go, sizeof(GameObject*));
        ImGui::Text("%s", go->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // Drag & Drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
            IM_ASSERT(payload->DataSize == sizeof(GameObject*));
            GameObject* draggedGO = *(GameObject**)payload->Data;

            // Don't allow dropping onto itself or any of its own children
            if (draggedGO != go && !IsChildOf(draggedGO, go)) {
                m_ToReparent.push_back({ draggedGO, go });
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (open && !go->GetChildren().empty()) {
        for (GameObject* child : go->GetChildren()) {
            DrawGameObjectNode(child);
        }
        ImGui::TreePop();
    }
}

bool HierarchyWindow::IsChildOf(GameObject* parent, GameObject* child) {
    GameObject* current = child->GetParent();
    while (current) {
        if (current == parent) return true;
        current = current->GetParent();
    }
    return false;
}
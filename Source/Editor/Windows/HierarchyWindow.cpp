#include "HierarchyWindow.h"

#include "ECS/Scene.h"
#include "ECS/GameObject.h"

#include "Core/Application.h"
#include "Modules/ModuleScene.h"

HierarchyWindow::HierarchyWindow(const char* title, bool start_open)
        : IEditorWindow(title, nullptr, start_open) {
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

                if (opened) {

                    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
                        if (!ImGui::IsAnyItemHovered()) {
                            m_Selected = nullptr;
                            // Optionally notify the application/inspector:
                            External->scene->selectedGameObject = m_Selected;
                        }
                    }

                    // Draw only root-level objects
                    for (auto& goPtr : m_Scene->GetGameObjects())
                    {
                        GameObject* obj = goPtr.get();
                        if (!obj->GetParent()) {
                            DrawGameObjectNode(obj);
                        }
                    }
                    ImGui::TreePop();
                }

                // Process deletion first
                for (auto* go : m_ToDelete) {
                    if (External->scene->selectedGameObject == go) {
                        m_Selected = nullptr;
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
    if (go == m_Selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    std::string label = go->GetName() + "###" + std::to_string(go->GetID());
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    // Left-click to select
    if (ImGui::IsItemClicked()) {
        m_Selected = go;
        // Optionally notify the application/inspector:
        External->scene->selectedGameObject = m_Selected;
    }

    // Right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            m_ToDelete.push_back(go);
            if (m_Selected == go)
                m_Selected = nullptr; // clear selection if deleted
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
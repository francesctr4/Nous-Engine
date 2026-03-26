#include "Editor/UI/Windows/HierarchyWindow/include/HierarchyWindow.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Systems/PrefabManager/include/PrefabManager.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Core/Logger/Logger.h"

#include "imgui.h"

#include <filesystem>
#include <string>

HierarchyWindow::HierarchyWindow(const char* title, EditorContext* context, bool start_open)
        : IEditorWindow(title, context, nullptr, start_open),
        m_ToDelete(MemoryTag::SCENE), m_ToReparent(MemoryTag::SCENE)
{
    Init();
}

void HierarchyWindow::Init()
{
    SetScene(External->GetScene()->activeScene);
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
                                                "%s", m_Scene->GetName().c_str());

                // Make the SCENE NODE a drag-drop target (reparent to root or instantiate prefab)
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
                        IM_ASSERT(payload->DataSize == sizeof(GameObject*));
                        GameObject* draggedGO = *(GameObject**)payload->Data;
                        m_ToReparent.push_back({ draggedGO, nullptr });
                    }
                    // Accept .nprefab files dragged from the AssetsBrowser
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS")) {
                        const char* data = static_cast<const char*>(payload->Data);
                        const char* end  = data + payload->DataSize;
                        while (data < end) {
                            std::string path(data);
                            data += path.size() + 1;
                            if (std::filesystem::path(path).extension() == ".nprefab")
                                External->GetScene()->InstantiatePrefab(path, nullptr);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // Right-click on the scene root to create objects.
                if (ImGui::BeginPopupContextItem("##SceneContextMenu")) {
                    if (ImGui::MenuItem("Create Empty")) {
                        GameObject* go = m_Scene->CreateGameObject("GameObject", nullptr);
                        External->GetScene()->selectedGameObject = go;
                    }
                    if (ImGui::MenuItem("Create Camera")) {
                        GameObject* go = m_Scene->CreateGameObject("Main Camera", nullptr);
                        auto& cam = go->AddComponent<CCamera>();
                        cam.isMainCamera = true;
                        External->GetScene()->selectedGameObject = go;
                    }
                    ImGui::EndPopup();
                }

                if (opened) {
                    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
                        if (!ImGui::IsAnyItemHovered()) {
                            External->GetScene()->selectedGameObject = nullptr;
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
                    if (External->GetScene()->selectedGameObject == go) {
                        External->GetScene()->selectedGameObject = nullptr;
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

        // Drawn outside Begin/End so the modal can overlap the window
        DrawSaveAsPrefabPopup();
    }
}

void HierarchyWindow::DrawGameObjectNode(GameObject* go, bool insidePrefab) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;

    if (go->GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Highlight if selected
    if (go == External->GetScene()->selectedGameObject)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Blue tint for prefab instance roots and all their children
    const bool isPrefab = go->HasComponent<CPrefab>();
    const bool applyTint = isPrefab || insidePrefab;
    if (applyTint)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));

    std::string label = go->GetName() + "###" + std::to_string(go->GetID());
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    if (applyTint)
        ImGui::PopStyleColor();

    // Left-click to select
    if (ImGui::IsItemClicked()) {
        External->GetScene()->selectedGameObject = go;
    }

    // Right-click menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            m_ToDelete.push_back(go);
            if (External->GetScene()->selectedGameObject == go)
                External->GetScene()->selectedGameObject = nullptr;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save As Prefab")) {
            m_prefabSaveTarget = go;
            // Pre-fill the name buffer with the GO name
            strncpy(m_prefabNameBuffer, go->GetName().c_str(), sizeof(m_prefabNameBuffer) - 1);
            m_prefabNameBuffer[sizeof(m_prefabNameBuffer) - 1] = '\0';
            m_showSaveAsPrefabPopup = true;
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
        // Accept .nprefab files dragged from the AssetsBrowser (instantiate as child of go)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS")) {
            const char* data = static_cast<const char*>(payload->Data);
            const char* end  = data + payload->DataSize;
            while (data < end) {
                std::string path(data);
                data += path.size() + 1;
                if (std::filesystem::path(path).extension() == ".nprefab")
                    External->GetScene()->InstantiatePrefab(path, go);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (open && !go->GetChildren().empty()) {
        for (GameObject* child : go->GetChildren()) {
            DrawGameObjectNode(child, applyTint);
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

void HierarchyWindow::DrawSaveAsPrefabPopup()
{
    if (m_showSaveAsPrefabPopup)
    {
        ImGui::OpenPopup("Save As Prefab");
        m_showSaveAsPrefabPopup = false;
    }

    if (ImGui::BeginPopupModal("Save As Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Prefab name:");
        ImGui::SetNextItemWidth(300.0f);
        const bool enterPressed = ImGui::InputText("##PrefabName", m_prefabNameBuffer,
                                                   sizeof(m_prefabNameBuffer),
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const bool nameEmpty = strlen(m_prefabNameBuffer) == 0;
        if (nameEmpty) ImGui::BeginDisabled();

        if (ImGui::Button("Save", ImVec2(120, 0)) || enterPressed)
        {
            if (!nameEmpty && m_prefabSaveTarget)
            {
                std::string filePath = std::string("Assets/Prefabs/") + m_prefabNameBuffer + ".nprefab";
                PrefabManager::SavePrefab(m_prefabSaveTarget, filePath);

                // Mark this GO as a prefab instance immediately.
                // AddComponent replaces any existing CPrefab if the GO was already one.
                auto& cprefab = m_prefabSaveTarget->AddComponent<CPrefab>();
                cprefab.prefabSourcePath = filePath;

                NOUS_INFO("[HierarchyWindow] Saved prefab: %s", filePath.c_str());
            }
            memset(m_prefabNameBuffer, 0, sizeof(m_prefabNameBuffer));
            m_prefabSaveTarget = nullptr;
            ImGui::CloseCurrentPopup();
        }

        if (nameEmpty) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            memset(m_prefabNameBuffer, 0, sizeof(m_prefabNameBuffer));
            m_prefabSaveTarget = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
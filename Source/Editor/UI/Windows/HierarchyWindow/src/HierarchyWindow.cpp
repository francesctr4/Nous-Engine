#include "Editor/UI/Windows/HierarchyWindow/include/HierarchyWindow.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Systems/PrefabManager/include/PrefabManager.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Core/Logger/Logger.h"

#include "imgui.h"

#include <filesystem>
#include <string>

HierarchyWindow::HierarchyWindow(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void HierarchyWindow::Init()
{
    SetScene(editorContext->GetScene()->activeScene);
}

void HierarchyWindow::DrawContent()
{
    if (m_Scene)
    {
        // Root node = Scene itself
        const bool opened = ImGui::TreeNodeEx(m_Scene,
                                              ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow,
                                              "%s", m_Scene->GetName().c_str());

        // Make the SCENE NODE a drag-drop target (reparent to root or instantiate prefab)
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
            {
                IM_ASSERT(payload->DataSize == sizeof(entt::entity));
                entt::entity e = *static_cast<const entt::entity*>(payload->Data);
                GameObject draggedGO(e, &m_Scene->GetRegistry());
                if (draggedGO.IsValid())
                    m_ToReparent.push_back({draggedGO, {}});
            }
            // Accept .nprefab files dragged from the AssetsBrowser
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
            {
                auto data = static_cast<const char*>(payload->Data);
                const char* end = data + payload->DataSize;
                while (data < end)
                {
                    std::string path(data);
                    data += path.size() + 1;
                    if (std::filesystem::path(path).extension() == ".nprefab")
                        editorContext->GetScene()->InstantiatePrefab(path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Right-click on the scene root to create objects.
        if (ImGui::BeginPopupContextItem("##SceneContextMenu"))
        {
            if (ImGui::MenuItem("Create Empty"))
            {
                GameObject go = m_Scene->CreateGameObject("GameObject", nullptr);
                editorContext->GetScene()->selectedGameObject = go;
            }
            if (ImGui::MenuItem("Create Camera"))
            {
                GameObject go = m_Scene->CreateGameObject("Main Camera", nullptr);
                auto& cam = go.AddComponent<CCamera>();
                cam.isMainCamera = true;
                editorContext->GetScene()->selectedGameObject = go;
            }
            if (ImGui::MenuItem("Create Light"))
            {
                GameObject go = m_Scene->CreateGameObject("Directional Light", nullptr);
                go.AddComponent<CLight>();
                editorContext->GetScene()->selectedGameObject = go;
            }
            ImGui::EndPopup();
        }

        if (opened)
        {
            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
            {
                if (!ImGui::IsAnyItemHovered())
                {
                    editorContext->GetScene()->selectedGameObject = {};
                }
            }

            for (const auto go : m_Scene->GetGameObjectsSnapshot())
            {
                if (!go.GetParent().IsValid())
                    DrawGameObjectNode(go);
            }
            ImGui::TreePop();
        }

        // Process deletion first
        for (auto go : m_ToDelete)
        {
            if (editorContext->GetScene()->selectedGameObject == go)
                editorContext->GetScene()->selectedGameObject = {};
            m_Scene->DestroyGameObject(go);
        }
        m_ToDelete.clear();

        // Process reparenting safely
        for (auto& req : m_ToReparent)
        {
            req.child.SetParent(req.newParent);
        }
        m_ToReparent.clear();
    }
}

void HierarchyWindow::FinishUpdate()
{
    // Drawn outside Begin/End so the modal can overlap the window
    DrawSaveAsPrefabPopup();
}

void HierarchyWindow::DrawGameObjectNode(GameObject obj, const bool insidePrefab)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;

    if (obj.GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (obj == editorContext->GetScene()->selectedGameObject)
        flags |= ImGuiTreeNodeFlags_Selected;

    const bool isPrefab = obj.HasComponent<CPrefab>();
    const bool applyTint = isPrefab || insidePrefab;
    if (applyTint)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));

    const std::string label = obj.GetName() + "###" + std::to_string(obj.GetID());
    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    if (applyTint)
        ImGui::PopStyleColor();

    if (ImGui::IsItemClicked())
        editorContext->GetScene()->selectedGameObject = obj;

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))
        {
            m_ToDelete.push_back(obj);
            if (editorContext->GetScene()->selectedGameObject == obj)
                editorContext->GetScene()->selectedGameObject = {};
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save As Prefab"))
        {
            m_prefabSaveTarget = obj;
            strncpy(m_prefabNameBuffer, obj.GetName().c_str(), sizeof(m_prefabNameBuffer) - 1);
            m_prefabNameBuffer[sizeof(m_prefabNameBuffer) - 1] = '\0';
            m_showSaveAsPrefabPopup = true;
        }
        ImGui::EndPopup();
    }

    // Drag source — transfer entity ID through payload
    if (ImGui::BeginDragDropSource())
    {
        entt::entity e = obj.GetEntity();
        ImGui::SetDragDropPayload("DND_GAMEOBJECT", &e, sizeof(entt::entity));
        ImGui::Text("%s", obj.GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // Drag & Drop target
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            IM_ASSERT(payload->DataSize == sizeof(entt::entity));
            entt::entity e = *static_cast<const entt::entity*>(payload->Data);
            GameObject draggedGO(e, &m_Scene->GetRegistry());
            if (draggedGO.IsValid() && draggedGO != obj && !IsChildOf(obj, draggedGO))
                m_ToReparent.push_back({draggedGO, obj});
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            auto data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end)
            {
                std::string path(data);
                data += path.size() + 1;
                if (std::filesystem::path(path).extension() == ".nprefab")
                    editorContext->GetScene()->InstantiatePrefab(path, obj);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (open && !obj.GetChildren().empty())
    {
        for (const auto child : obj.GetChildren())
            DrawGameObjectNode(child, applyTint);
        ImGui::TreePop();
    }
}

bool HierarchyWindow::IsChildOf(GameObject parent, GameObject child)
{
    GameObject current = child.GetParent();
    while (current.IsValid())
    {
        if (current == parent) return true;
        current = current.GetParent();
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
            if (!nameEmpty && m_prefabSaveTarget.IsValid())
            {
                const std::string filePath = std::string("Assets/Prefabs/") + m_prefabNameBuffer + ".nprefab";
                PrefabManager::SavePrefab(m_prefabSaveTarget, filePath);

                auto& cprefab = m_prefabSaveTarget.AddComponent<CPrefab>();
                cprefab.prefabSourcePath = filePath;

                NOUS_INFO("[HierarchyWindow] Saved prefab: %s", filePath.c_str());
            }
            memset(m_prefabNameBuffer, 0, sizeof(m_prefabNameBuffer));
            m_prefabSaveTarget = {};
            ImGui::CloseCurrentPopup();
        }

        if (nameEmpty) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            memset(m_prefabNameBuffer, 0, sizeof(m_prefabNameBuffer));
            m_prefabSaveTarget = {};
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

#include "InspectorWindow.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/Component/Types/CPrefab/include/CPrefab.h"
#include "Engine/Systems/ECS/Component/Types/ComponentTypes.h"
#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/ComponentInspectorRegistry/include/ComponentInspectorRegistry.h"

#include "imgui.h"
#include <string>
#include <string_view>
#include <unordered_set>

InspectorWindow::InspectorWindow(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void InspectorWindow::DrawContent()
{
    const ModuleScene* mScene = editorContext->GetScene();

    if (mScene->selectedGameObjects.size() > 1)
    {
        ImGui::TextDisabled("%zu objects selected.", mScene->selectedGameObjects.size());
        return;
    }

    GameObject go = mScene->primarySelection;
    if (!go.IsValid())
    {
        ImGui::TextDisabled("No GameObject selected.");
        return;
    }

    DrawGameObjectHeader(&go);

    ImGui::Spacing();

    InspectorCtx ctx{};
    ctx.go            = &go;
    ctx.editor        = editorContext;
    ctx.scene         = mScene->activeScene;
    ctx.rm            = go.GetScene()->GetResourceManager();
    ctx.renderer      = editorContext->GetRendererFrontend();
    ctx.scriptManager = mScene->scriptManager;

    for (Component* c : go.GetAllComponents())
        if (const ComponentUI* e = FindComponentUI(c->GetType()))
            e->draw(ctx, c);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    DrawAddComponentSection(&go);
}

void InspectorWindow::DrawGameObjectHeader(GameObject* go)
{
    // --- GameObject Header ---
    auto const* cprefab = go->TryGetComponent<CPrefab>();
    if (cprefab)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        ImGui::SeparatorText("Prefab Instance");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
        ImGui::TextUnformatted(cprefab->prefabSourcePath.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    else
    {
        ImGui::SeparatorText("GameObject Info");
    }

    const uint32_t currentID = go->GetID();
    if (currentID != m_lastSelectedID)
    {
        m_nameBuffer = go->GetName();
        m_lastSelectedID = currentID;
    }
    m_nameBuffer.resize(256);

    if (cprefab)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));

    if (ImGui::InputText("##Name", m_nameBuffer.data(), m_nameBuffer.size()))
    {
        go->SetName(m_nameBuffer);
    }

    if (cprefab)
        ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("(ID: %u)", go->GetID());
}

void InspectorWindow::DrawAddComponentSection(GameObject* go) const
{
    // Names of components already attached — skip them in the menu.
    std::unordered_set<std::string> present;
    for (Component* c : go->GetAllComponents())
        present.insert(std::string(c->GetType()));

    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("##AddComponentPopup");

    if (ImGui::BeginPopup("##AddComponentPopup"))
    {
        for (const ComponentUI& e : ComponentUITable())
        {
            if (!e.userAddable || present.count(std::string(e.typeName)))
                continue;
            if (ImGui::MenuItem(e.displayName))
                ComponentTypes::AddByName(*go, std::string(e.typeName));
        }
        ImGui::EndPopup();
    }
}

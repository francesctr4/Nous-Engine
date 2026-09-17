#include <EditorUI/InspectorWindow.h>
#include <ModuleScene/ModuleScene.h>
#include <ModuleResourceManager/ModuleResourceManager.h>

#include <ECS/GameObject.h>
#include <ECS/Scene/Scene.h>
#include <ECS/Component/Types/CPrefab/CPrefab.h>
#include <ECS/Component/Types/ComponentTypes.h>
#include "Windows/InspectorWindow/InspectorComponent/ComponentInspectorRegistry/ComponentInspectorRegistry.h"

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
    ctx.rm            = editorContext->GetResourceManager();
    ctx.renderer      = editorContext->GetRendererFrontend();
    ctx.scriptManager = mScene->GetScriptManager();

    // Push a per-component ImGui ID scope so identically-labelled widgets in
    // different components (e.g. "Loop" / "Play On Awake" on both CVideoPlayer
    // and an auto-paired CAudioSource) don't collide into one shared ID.
    for (Component* c : go.GetAllComponents())
        if (const ComponentUI* e = FindComponentUI(c->GetType()))
        {
            ImGui::PushID(c);
            e->draw(ctx, c);
            ImGui::PopID();
        }

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

    // SCOPED TO THE GAMEOBJECT, and this is load-bearing rather than tidiness. ImGui
    // identifies a widget by ID, not by which object the caller had in mind, and an
    // InputText that loses focus stashes its text to be re-applied ON A LATER FRAME by
    // whatever widget claims the same ID (imgui_widgets.cpp, "Handle reapplying final
    // data on deactivation"). With one shared "##Name", typing into object A and then
    // clicking object B in the Hierarchy applied A's text to B -- the rename followed
    // the selection. The edit itself never needed that path: InputText returns true on
    // every keystroke, so SetName has already run by then.
    ImGui::PushID(static_cast<int>(currentID));

    if (ImGui::InputText("##Name", m_nameBuffer.data(), m_nameBuffer.size()))
    {
        // Up to the terminator, NOT the whole 256-byte edit buffer: m_nameBuffer is a
        // std::string resized to the widget's capacity, so its size() still counts the
        // padding ImGui left behind it. Handing that straight to SetName gives every
        // renamed object a name with embedded NULs, which displays fine through c_str()
        // and quietly fails every std::string comparison against it.
        go->SetName(m_nameBuffer.c_str());
    }

    ImGui::PopID();

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

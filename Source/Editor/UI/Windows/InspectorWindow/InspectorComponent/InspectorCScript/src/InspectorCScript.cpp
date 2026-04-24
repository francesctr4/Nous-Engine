#include <array>
#include <string>

#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/InspectorCScript/include/InspectorCScript.h"

#include "Editor/UI/Windows/TextEditorWindow/include/TextEditorWindow.h"
#include "Engine/Scripting/ScriptManager.h"
#include "Engine/Scripting/Internal/IScript.inl"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void DrawAttachedScripts(Scene* scene, CScript* cscript)
{
    const auto& scriptNames = cscript->GetScriptNames();

    // One sub-section per attached script
    for (int i = 0; i < static_cast<int>(scriptNames.size()); ++i)
    {
        if (DrawSingleScript(scene, cscript, i, scriptNames[i]))
            break;
    }
}

bool DrawSingleScript(Scene* scene, CScript* cscript, int i, const std::string& sName)
{
    // Script header with remove button.
    // Render the Remove button first (right-aligned) so it gets
    // click priority over the CollapsingHeader that spans the row.
    ImGui::PushID(i);

    float removeWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - removeWidth);
    if (ImGui::SmallButton("Remove"))
    {
        cscript->RemoveScript(sName);
        ImGui::PopID();
        return true; // iterator invalidated — skip rest of frame
    }
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - availWidth);

    // Properties (if script has any)
    if (ImGui::CollapsingHeader(sName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        DrawScriptProperties(scene, cscript, i);

    ImGui::PopID();
    return false;
}

void DrawScriptProperties(Scene* scene, const CScript* cscript, int i)
{
    IScript* inst = cscript->GetInstance(i);
    if (inst)
    {
        ImGui::Indent();
        auto props = inst->GetProperties();
        if (props.empty())
        {
            ImGui::TextDisabled("No exposed properties.");
        }
        for (const auto& prop : props)
        {
            DrawScriptProperty(scene, prop);
        }
        ImGui::Unindent();
    }
}

void DrawScriptProperty(Scene* scene, const ScriptProperty& prop)
{
    switch (prop.type)
    {
        using enum ScriptProperty::Type;
    case Float:
        ImGui::DragFloat(prop.name, static_cast<float*>(prop.ptr), 0.1f);
        break;
    case Int:
        ImGui::DragInt(prop.name, static_cast<int*>(prop.ptr));
        break;
    case Bool:
        ImGui::Checkbox(prop.name, static_cast<bool*>(prop.ptr));
        break;
    case GameObject:
        DrawGameObjectProperty(scene, prop);
        break;
    case String:
    {
        auto* strPtr = static_cast<std::string*>(prop.ptr);
        std::array<char, 256> buf{};
        strPtr->copy(buf.data(), buf.size() - 1);
        if (ImGui::InputText(prop.name, buf.data(), buf.size()))
            *strPtr = buf.data();
        break;
    }
    }
}

void DrawGameObjectProperty(Scene* scene, const ScriptProperty& prop)
{
    auto* idPtr = static_cast<uint32_t*>(prop.ptr);

    // Resolve current name for the preview label
    if (std::string preview = GetGameObjectPreview(idPtr, scene);
        !ImGui::BeginCombo(prop.name, preview.c_str()))
        return;

    // None option
    if (ImGui::Selectable("None", *idPtr == 0))
        *idPtr = 0;

    // All scene GameObjects
    DrawGameObjectOptions(idPtr, scene);

    ImGui::EndCombo();
}

std::string GetGameObjectPreview(const uint32_t* idPtr, Scene* scene)
{
    if (*idPtr == 0 || !scene)
        return "None";

    auto target = scene->GetGameObjectByID(*idPtr);
    return target.IsValid() ? target.GetName() : "(missing)";
}

void DrawGameObjectOptions(uint32_t* idPtr, const Scene* scene)
{
    if (!scene) return;

    const auto gos = scene->GetGameObjectsSnapshot();

    for (auto target : gos)
    {
        const bool selected = (*idPtr == target.GetID());

        if (ImGui::Selectable(target.GetName().c_str(), selected))
            *idPtr = target.GetID();

        if (selected)
            ImGui::SetItemDefaultFocus();
    }
}

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void DrawAddScriptSection(const ScriptManager* scriptManager, CScript* cscript)
{
    const std::vector<std::string> available = scriptManager->GetAvailableScriptNames();

    if (available.empty())
    {
        ImGui::TextDisabled("No scripts in registry (rebuild DLL?)");
        return;
    }

    static int s_selectedScript = 0;
    if (s_selectedScript >= static_cast<int>(available.size()))
        s_selectedScript = 0;

    std::vector<const char*> cNames;
    cNames.reserve(available.size());
    for (const auto& n : available) cNames.push_back(n.c_str());

    ImGui::SetNextItemWidth(160.0f);
    ImGui::Combo("##ScriptPick", &s_selectedScript,
                 cNames.data(), static_cast<int>(cNames.size()));
    ImGui::SameLine();
    if (ImGui::Button("Add Script"))
        cscript->AddScript(available[s_selectedScript]);
}

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

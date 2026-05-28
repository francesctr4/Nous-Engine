#include "InspectorWindow.h"
#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/InspectorCMaterial/include/InspectorCMaterial.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Scripting/Internal/IScript.inl"

#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/MetaFileData.h"

#include "imgui.h"
#include <glm/glm.hpp>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/InspectorCScript/include/InspectorCScript.h"

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

    if (go.HasComponent<CTransform>())
        DrawTransformComponent(&go.GetComponent<CTransform>());

    if (go.HasComponent<CMesh>())
        DrawMeshComponent(&go.GetComponent<CMesh>());

    if (go.HasComponent<CCamera>())
        DrawCameraComponent(&go.GetComponent<CCamera>());

    if (go.HasComponent<CLight>())
        DrawLightComponent(&go.GetComponent<CLight>());

    if (go.HasComponent<CMaterial>())
        DrawMaterialComponent(&go, &go.GetComponent<CMaterial>(),
            go.GetScene()->GetResourceManager(), editorContext->GetRendererFrontend());

    if (go.HasComponent<CScript>())
        DrawScriptComponent(mScene->activeScene, mScene->scriptManager, &go.GetComponent<CScript>());

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

void InspectorWindow::DrawTransformComponent(CTransform* ctransform) const
{
    // --- Transform Component ---
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    // Position
    if (ImGui::DragFloat3("Position", &ctransform->position.x, 0.1f))
    {
        ctransform->MarkDirty();
        ctransform->UpdateMatrix();
    }

    // Rotation (edit via Euler hint, convert to quaternion on change)
    if (ImGui::DragFloat3("Rotation (Euler)", &ctransform->eulerHint.x, 0.1f))
    {
        ctransform->SetEulerRotation(ctransform->eulerHint);
        ctransform->UpdateMatrix();
    }

    // Scale
    if (ImGui::DragFloat3("Scale", &ctransform->scale.x, 0.01f, 0.001f, 100.0f))
    {
        ctransform->MarkDirty();
        ctransform->UpdateMatrix();
    }

    ImGui::Unindent();
}

void InspectorWindow::DrawMeshComponent(const CMesh* cmesh) const
{
    // --- Mesh Component ---
    if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (!cmesh->mesh)
    {
        ImGui::TextDisabled("No mesh assigned.");
        return;
    }

    ImGui::Text("Name: %s", cmesh->mesh->GetName().c_str());
    ImGui::Text("UID: %u", cmesh->mesh->GetUID());
    ImGui::Text("Assets Path: %s", cmesh->mesh->GetAssetsPath().c_str());
    ImGui::Text("Library Path: %s", cmesh->mesh->GetLibraryPath().c_str());

    ImGui::Text("Vertices: %zu", cmesh->mesh->vertices.size());
    ImGui::Text("Indices: %zu", cmesh->mesh->indices.size());
}

void InspectorWindow::DrawCameraComponent(CCamera* ccamera) const
{
    // --- Camera Component ---
    if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    ImGui::Checkbox("Main Camera", &ccamera->isMainCamera);

    if (ImGui::DragFloat("FOV", &ccamera->fov, 0.5f, 5.0f, 170.0f, "%.1f deg"))
        ccamera->fov = glm::clamp(ccamera->fov, 5.0f, 170.0f);

    if (ImGui::DragFloat("Near Plane", &ccamera->nearPlane, 0.01f, 0.01f, ccamera->farPlane - 0.01f, "%.3f"))
        ccamera->nearPlane = glm::max(ccamera->nearPlane, 0.001f);

    if (ImGui::DragFloat("Far Plane", &ccamera->farPlane, 1.0f, ccamera->nearPlane + 0.01f, 100000.0f, "%.1f"))
        ccamera->farPlane = glm::max(ccamera->farPlane, ccamera->nearPlane + 0.01f);
    
    ImGui::Unindent();
}

void InspectorWindow::DrawLightComponent(CLight* clight) const
{
    // --- Light Component ---
    if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    static constexpr std::array<const char*, 3> lightTypeNames = {
        "Directional",
        "Point",
        "Spot"
    };

    if (int currentType = std::to_underlying(clight->type);
        ImGui::Combo("Type", &currentType, lightTypeNames.data(), lightTypeNames.size()))
    {
        clight->type = static_cast<LightType>(currentType);
    }

    ImGui::ColorEdit3("Color", &clight->color.r);

    ImGui::DragFloat("Intensity", &clight->intensity, 0.01f, 0.0f, 100.0f, "%.2f");
    clight->intensity = glm::max(clight->intensity, 0.0f);

    if (clight->type == LightType::Point)
    {
        ImGui::DragFloat("Range", &clight->range, 0.1f, 0.1f, 10000.0f, "%.1f");
        clight->range = glm::max(clight->range, 0.1f);
    }

    if (clight->type == LightType::Spot)
    {
        ImGui::DragFloat("Range", &clight->range, 0.1f, 0.1f, 10000.0f, "%.1f");
        clight->range = glm::max(clight->range, 0.1f);

        ImGui::DragFloat("Inner Angle", &clight->innerAngle, 0.5f, 1.0f, 89.0f, "%.1f deg");
        clight->innerAngle = glm::clamp(clight->innerAngle, 1.0f, 89.0f);

        ImGui::DragFloat("Outer Angle", &clight->outerAngle, 0.5f, 1.0f, 90.0f, "%.1f deg");
        clight->outerAngle = glm::clamp(clight->outerAngle, clight->innerAngle + 0.5f, 90.0f);
    }

    ImGui::Unindent();
}

void InspectorWindow::DrawMaterialComponent(const GameObject* go, CMaterial* cmaterial,
    ModuleResourceManager* rm, RendererFrontend* rendererFrontend) const
{
    // --- Material Component ---
    if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (!cmaterial->material)
        return;

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::Button(cmaterial->material->GetName().c_str(), ImVec2(200.0f, 0.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            const auto* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end)
            {
                std::string path(data);
                data += path.size() + 1;
                if (std::filesystem::path(path).extension() == ".nmat")
                {
                    Resource* r = rm->CreateResource(path);
                    if (r)
                    {
                        if (cmaterial->material && cmaterial->material != rm->GetDefaultMaterial())
                            rm->UnloadResource(cmaterial->material->GetUID());
                        cmaterial->material = down_cast<ResourceMaterial*>(r);
                    }
                    break;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::Text("UID: %u", cmaterial->material->GetUID());
    ImGui::Text("Assets Path: %s", cmaterial->material->GetAssetsPath().c_str());
    ImGui::Text("Library Path: %s", cmaterial->material->GetLibraryPath().c_str());

    if (cmaterial->material == rm->GetDefaultMaterial())
    {
        CMaterialDrawDefaultMaterial(go, *cmaterial, rm, editorContext->GetAssetsBrowserDirectory());
        return;
    }

    CMaterialDrawShaderSelector(rendererFrontend, *cmaterial, rm);

    // Determine effective shader: custom if assigned, otherwise built-in.
    // Resolved once and shared between the Uniforms and Texture Maps sections.
    ResourceShader* effectiveShader = GetEffectiveShader(*cmaterial, rm);

    CMaterialDrawShaderUniforms(*cmaterial, effectiveShader);

    CMaterialDrawTextureMaps(*cmaterial, rm, effectiveShader);

    if (ImGui::Button("Save Material"))
    {
        ImporterMaterial::SaveMaterialToAssets(cmaterial->material);
    }
}

void InspectorWindow::DrawScriptComponent(Scene* scene, const ScriptManager* scriptManager, CScript* cscript) const
{
    // --- Script Component ---
    if (!ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    DrawAttachedScripts(scene, cscript);

    ImGui::Spacing();

    // Add script from dropdown
    DrawAddScriptSection(scriptManager, cscript);

    ImGui::Unindent();
}

void InspectorWindow::DrawAddComponentSection(GameObject* go) const
{
    // --- Add Component section ---
    if (!go->HasComponent<CScript>() && ImGui::Button("Add CScript Component"))
        go->AddComponent<CScript>();
}

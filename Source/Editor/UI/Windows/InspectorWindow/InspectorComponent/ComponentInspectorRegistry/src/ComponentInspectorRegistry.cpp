#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/ComponentInspectorRegistry/include/ComponentInspectorRegistry.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/InspectorCMaterial/include/InspectorCMaterial.h"
#include "Editor/UI/Windows/InspectorWindow/InspectorComponent/InspectorCScript/include/InspectorCScript.h"

#include "Engine/Core/Globals.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/Types/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/Types/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/Types/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/Types/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/Types/CScript/include/CScript.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"

#include "imgui.h"
#include <glm/glm.hpp>
#include <array>
#include <filesystem>
#include <string>
#include <utility>

// ─────────────────────────────────────────────────────────────────────────────
// Drawers — one per inspectable component type. Uniform signature so they can be
// stored in the registry table. Each casts the Component* to its concrete type
// and pulls only the dependencies it needs from InspectorCtx.
// ─────────────────────────────────────────────────────────────────────────────

static void DrawTransform(const InspectorCtx&, Component* c)
{
    auto* ctransform = static_cast<CTransform*>(c);

    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    if (ImGui::DragFloat3("Position", &ctransform->position.x, 0.1f))
    {
        ctransform->MarkDirty();
        ctransform->UpdateMatrix();
    }

    if (ImGui::DragFloat3("Rotation (Euler)", &ctransform->eulerHint.x, 0.1f))
    {
        ctransform->SetEulerRotation(ctransform->eulerHint);
        ctransform->UpdateMatrix();
    }

    if (ImGui::DragFloat3("Scale", &ctransform->scale.x, 0.01f, 0.001f, 100.0f))
    {
        ctransform->MarkDirty();
        ctransform->UpdateMatrix();
    }

    ImGui::Unindent();
}

static void DrawMesh(const InspectorCtx&, Component* c)
{
    auto* cmesh = static_cast<CMesh*>(c);

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

static void DrawCamera(const InspectorCtx&, Component* c)
{
    auto* ccamera = static_cast<CCamera*>(c);

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

static void DrawLight(const InspectorCtx&, Component* c)
{
    auto* clight = static_cast<CLight*>(c);

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

static void DrawMaterial(const InspectorCtx& ctx, Component* c)
{
    auto* cmaterial = static_cast<CMaterial*>(c);
    const GameObject*      go               = ctx.go;
    ModuleResourceManager* rm               = ctx.rm;
    RendererFrontend*      rendererFrontend = ctx.renderer;

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
        CMaterialDrawDefaultMaterial(go, *cmaterial, rm, ctx.editor->GetAssetsBrowserDirectory());
        return;
    }

    CMaterialDrawShaderSelector(rendererFrontend, *cmaterial, rm);

    // Determine effective shader: custom if assigned, otherwise built-in.
    ResourceShader* effectiveShader = GetEffectiveShader(*cmaterial, rm);

    CMaterialDrawShaderUniforms(*cmaterial, effectiveShader);

    CMaterialDrawTextureMaps(*cmaterial, rm, effectiveShader);

    if (ImGui::Button("Save Material"))
    {
        ImporterMaterial::SaveMaterialToAssets(cmaterial->material);
    }
}

static void DrawScript(const InspectorCtx& ctx, Component* c)
{
    auto* cscript = static_cast<CScript*>(c);

    if (!ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    DrawAttachedScripts(ctx.scene, cscript);

    ImGui::Spacing();

    DrawAddScriptSection(ctx.scriptManager, cscript);

    ImGui::Unindent();
}

// ─────────────────────────────────────────────────────────────────────────────
// THE editor edit-site: register a component type's inspector UI here.
// `typeName` must equal Component::GetType() (guarded by t_ComponentInspectorRegistry).
// ─────────────────────────────────────────────────────────────────────────────

static const ComponentUI k_ui[] = {
    {"CTransform", "Transform", false, &DrawTransform},
    {"CMesh",      "Mesh",      true,  &DrawMesh},
    {"CCamera",    "Camera",    true,  &DrawCamera},
    {"CLight",     "Light",     true,  &DrawLight},
    {"CMaterial",  "Material",  true,  &DrawMaterial},
    {"CScript",    "Script",    true,  &DrawScript},
};

const ComponentUI* FindComponentUI(std::string_view typeName)
{
    for (const ComponentUI& e : k_ui)
        if (e.typeName == typeName)
            return &e;
    return nullptr;
}

std::span<const ComponentUI> ComponentUITable()
{
    return k_ui;
}

#include "Editor/UI/Windows/InspectorWindow/include/InspectorWindow.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Scripting/ScriptManager.h"
#include "Engine/Scripting/Internal/IScript.inl"

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionTypes.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include "Engine/Core/Logger/Logger.h"

#include "imgui.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"

// Local mirror of VulkanBackend's DataType→UniformValueType converter: the
// Inspector lives in the Editor DLL and can't reach backend internals, and the
// mapping is trivial enough to duplicate.
static UniformValueType DataTypeToUniformValueType(DataType dt)
{
    switch (dt)
    {
        case DataType::Float: return UniformValueType::Float;
        case DataType::Vec2:  return UniformValueType::Vec2;
        case DataType::Vec3:  return UniformValueType::Vec3;
        case DataType::Vec4:  return UniformValueType::Vec4;
        case DataType::Int:   return UniformValueType::Int;
        case DataType::IVec2: return UniformValueType::IVec2;
        case DataType::IVec3: return UniformValueType::IVec3;
        case DataType::IVec4: return UniformValueType::IVec4;
        default:              return UniformValueType::Vec4;
    }
}

InspectorWindow::InspectorWindow(const char* title, EditorContext* context, const bool start_open)
        : IEditorWindow(title, context, nullptr, start_open) {
    Init();
}

void InspectorWindow::Init()
{

}

void InspectorWindow::Draw() {
    if (*p_open) {
        if (ImGui::Begin(title, p_open)) {

            GameObject* go = editorContext->GetScene()->selectedGameObject;
            if (!go) {
                ImGui::TextDisabled("No GameObject selected.");
                ImGui::End();
                return;
            }

            // --- GameObject Header ---
            auto* cprefab = go->TryGetComponent<CPrefab>();
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

            static char buffer[256];
            strncpy(buffer, go->GetName().c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            if (cprefab) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
                go->SetName(buffer);
            if (cprefab) ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("(ID: %u)", go->GetID());

            ImGui::Spacing();

            // --- Transform Component ---
            if (go->HasComponent<CTransform>()) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& transform = go->GetComponent<CTransform>();

                    ImGui::Indent();

                    // Position
                    if (ImGui::DragFloat3("Position", &transform.position.x, 0.1f))
                        transform.UpdateMatrix();

                    // Rotation (edit via Euler hint, convert to quaternion on change)
                    if (ImGui::DragFloat3("Rotation (Euler)", &transform.eulerHint.x, 0.1f))
                    {
                        transform.SetEulerRotation(transform.eulerHint);
                        transform.UpdateMatrix();
                    }

                    // Scale
                    if (ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.001f, 100.0f))
                        transform.UpdateMatrix();

                    ImGui::Unindent();
                }
            }

            // --- Mesh Component ---
            if (go->HasComponent<CMesh>()) {
                if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (auto& mesh = go->GetComponent<CMesh>();
                        mesh.mesh)
                    {
                        ImGui::Text("Name: %s", mesh.mesh->GetName().c_str());
                        ImGui::Text("UID: %u", mesh.mesh->GetUID());
                        ImGui::Text("Assets Path: %s", mesh.mesh->GetAssetsPath().c_str());
                        ImGui::Text("Library Path: %s", mesh.mesh->GetLibraryPath().c_str());

                        ImGui::Text("Vertices: %zu", mesh.mesh->vertices.size());
                        ImGui::Text("Indices: %zu", mesh.mesh->indices.size());
                    } else {
                        ImGui::TextDisabled("No mesh assigned.");
                    }
                }
            }

            // --- Camera Component ---
            if (go->HasComponent<CCamera>()) {
                if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& cam = go->GetComponent<CCamera>();

                    ImGui::Indent();

                    ImGui::Checkbox("Main Camera", &cam.isMainCamera);

                    if (ImGui::DragFloat("FOV", &cam.fov, 0.5f, 5.0f, 170.0f, "%.1f deg"))
                        cam.fov = glm::clamp(cam.fov, 5.0f, 170.0f);

                    if (ImGui::DragFloat("Near Plane", &cam.nearPlane, 0.01f, 0.01f, cam.farPlane - 0.01f, "%.3f"))
                        cam.nearPlane = glm::max(cam.nearPlane, 0.001f);

                    if (ImGui::DragFloat("Far Plane", &cam.farPlane, 1.0f, cam.nearPlane + 0.01f, 100000.0f, "%.1f"))
                        cam.farPlane = glm::max(cam.farPlane, cam.nearPlane + 0.01f);

                    ImGui::DragFloat("Aspect Ratio", &cam.aspectRatio, 0.01f, 0.1f, 10.0f, "%.3f");

                    ImGui::Unindent();
                }
            }

            // --- Light Component ---
            if (go->HasComponent<CLight>()) {
                if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& light = go->GetComponent<CLight>();

                    ImGui::Indent();

                    const char* lightTypeNames[] = { "Directional", "Point" };
                    int currentType = static_cast<int>(light.type);
                    if (ImGui::Combo("Type", &currentType, lightTypeNames, 2))
                        light.type = static_cast<LightType>(currentType);

                    ImGui::ColorEdit3("Color", &light.color.r);

                    ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
                    light.intensity = glm::max(light.intensity, 0.0f);

                    if (light.type == LightType::Point) {
                        ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 10000.0f, "%.1f");
                        light.range = glm::max(light.range, 0.1f);
                    }

                    ImGui::Unindent();
                }
            }

            // --- Material Component ---
            if (go->HasComponent<CMaterial>()) {
                if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (auto& mat = go->GetComponent<CMaterial>();
                        mat.material)
                    {
                        auto* rm = go->GetScene()->GetResourceManager();
                        const bool isDefaultMaterial = (mat.material == rm->GetDefaultMaterial());

                        ImGui::Text("Name: %s", mat.material->GetName().c_str());
                        ImGui::Text("UID: %u", mat.material->GetUID());
                        ImGui::Text("Assets Path: %s", mat.material->GetAssetsPath().c_str());
                        ImGui::Text("Library Path: %s", mat.material->GetLibraryPath().c_str());

                        if (isDefaultMaterial)
                        {
                            ImGui::Spacing();
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.33f, 1.0f));
                            ImGui::TextUnformatted("Using Default Material (read-only).");
                            ImGui::TextUnformatted("Create a material asset to assign textures / shaders that persist with the scene.");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();

                            if (ImGui::Button("Create Material Asset"))
                            {
                                // Build a safe filename from the GameObject name.
                                std::string safeName = go->GetName();
                                if (safeName.empty()) safeName = "Material";
                                for (char& c : safeName)
                                {
                                    // Replace characters invalid on Windows file systems with '_'.
                                    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                                        c == '"' || c == '<' || c == '>' || c == '|' || c == ' ')
                                        c = '_';
                                }

                                const std::string dir  = "Assets/Materials/";
                                std::string assetPath  = dir + safeName + "_material.nmat";

                                // Ensure uniqueness: append _1, _2, ... until we find a free path.
                                int suffix = 1;
                                while (std::filesystem::exists(assetPath))
                                {
                                    assetPath = dir + safeName + "_material_" + std::to_string(suffix++) + ".nmat";
                                    if (suffix > 1000) break; // safety guard
                                }

                                if (!ImporterMaterial::CreateNewMaterialFile(assetPath))
                                {
                                    NOUS_ERROR("InspectorWindow — failed to write new material file '%s'.", assetPath.c_str());
                                }
                                else if (!rm->ImportFile(assetPath))
                                {
                                    NOUS_ERROR("InspectorWindow — failed to import new material file '%s'.", assetPath.c_str());
                                }
                                else
                                {
                                    Resource* r = rm->CreateResource(assetPath);
                                    if (r)
                                        mat.material = down_cast<ResourceMaterial*>(r);
                                    else
                                        NOUS_ERROR("InspectorWindow — failed to load new material '%s'.", assetPath.c_str());
                                }
                            }
                        }
                        else
                        {

                        ImGui::SeparatorText("SHADER");

                        const std::string currentShaderLabel = (mat.material->shader == nullptr)
                            ? "Default (MaterialShader)"
                            : mat.material->shader->GetName();

                        if (ImGui::BeginCombo("Shader", currentShaderLabel.c_str()))
                        {
                            // Default entry — clears any custom shader assignment.
                            const bool defaultSelected = (mat.material->shader == nullptr);
                            if (ImGui::Selectable("Default (MaterialShader)", defaultSelected))
                            {
                                if (mat.material->shader != nullptr)
                                {
                                    rm->UnloadResource(mat.material->shader->GetUID());
                                    editorContext->GetRendererFrontend()->RequestMaterialShaderChange(
                                        mat.material, nullptr);

                                    // Drop all uniforms; the Uniforms section will re-populate
                                    // them next frame from the built-in MaterialShader reflection.
                                    mat.material->uniformValues.clear();
                                }
                            }
                            if (defaultSelected) ImGui::SetItemDefaultFocus();

                            // Scan Assets/Shaders/ for .glsl files.
                            const std::string shadersDir = "Assets/Shaders/";
                            if (std::filesystem::exists(shadersDir))
                            {
                                for (const auto& entry : std::filesystem::directory_iterator(shadersDir))
                                {
                                    if (!entry.is_regular_file()) continue;
                                    if (entry.path().extension() != ".glsl") continue;

                                    const std::string fullPath = entry.path().generic_string();
                                    const std::string fileName = entry.path().filename().string();
                                    const bool isSelected = (mat.material->shader != nullptr &&
                                                             mat.material->shader->GetAssetsPath() == fullPath);

                                    if (ImGui::Selectable(fileName.c_str(), isSelected))
                                    {
                                        Resource* r = rm->CreateResource(fullPath);
                                        if (r)
                                        {
                                            if (mat.material->shader != nullptr)
                                                rm->UnloadResource(mat.material->shader->GetUID());
                                            editorContext->GetRendererFrontend()->RequestMaterialShaderChange(
                                                mat.material, down_cast<ResourceShader*>(r));

                                            // Sync uniform values with the new shader's InstanceUBO:
                                            // keep entries whose names still exist (name-match strategy),
                                            // fill any missing members with defaults, and drop orphans.
                                            ResourceShader* newEffective = down_cast<ResourceShader*>(r);
                                            const auto& newSetIt =
                                                newEffective->reflection.descriptorSets.find(1);
                                            std::unordered_set<std::string> validNames;
                                            if (newSetIt != newEffective->reflection.descriptorSets.end())
                                            {
                                                for (const auto& rb : newSetIt->second)
                                                {
                                                    if (rb.type == DescriptorType::UniformBuffer && rb.binding == 0)
                                                    {
                                                        for (const auto& m : rb.members)
                                                        {
                                                            validNames.insert(m.name);
                                                            mat.material->uniformValues.try_emplace(
                                                                m.name,
                                                                UniformValue{
                                                                    DataTypeToUniformValueType(m.type),
                                                                    glm::vec4(1.0f) });
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                            // Prune orphaned entries.
                                            for (auto it = mat.material->uniformValues.begin();
                                                 it != mat.material->uniformValues.end(); )
                                            {
                                                if (validNames.find(it->first) == validNames.end())
                                                    it = mat.material->uniformValues.erase(it);
                                                else
                                                    ++it;
                                            }
                                        }
                                        else
                                        {
                                            NOUS_ERROR("InspectorWindow — failed to load shader '%s'.", fullPath.c_str());
                                        }
                                    }
                                    if (isSelected) ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }

                        // Determine effective shader: custom if assigned, otherwise built-in.
                        // Resolved once and shared between the Uniforms and Texture Maps sections.
                        ResourceShader* effectiveShader = mat.material->shader;
                        if (!effectiveShader)
                        {
                            // Use GetLoadedResource (no ref-count bump) — the built-in material
                            // shader is always loaded by the renderer, so we just borrow it.
                            static UID s_builtInMatShaderUID = 0;
                            if (s_builtInMatShaderUID == 0)
                            {
                                MetaFileData meta;
                                if (rm->GetAssetMetaData("Assets/Shaders/BuiltIn.MaterialShader.glsl", meta))
                                    s_builtInMatShaderUID = meta.uid;
                            }
                            if (s_builtInMatShaderUID != 0)
                                effectiveShader = down_cast<ResourceShader*>(
                                    rm->GetLoadedResource(s_builtInMatShaderUID));
                        }

                        // ── Uniforms section ─────────────────────────────────────────
                        // Mirrors the texture sampler discovery pattern: query set=1
                        // binding=0 UBO members from reflection, render widget per member.
                        if (effectiveShader && effectiveShader->GetState() == ResourceState::GPU_READY)
                        {
                            const auto& uniSetIt = effectiveShader->reflection.descriptorSets.find(1);
                            const ReflectedBinding* instanceUBO = nullptr;
                            if (uniSetIt != effectiveShader->reflection.descriptorSets.end())
                            {
                                for (const auto& rb : uniSetIt->second)
                                {
                                    if (rb.type == DescriptorType::UniformBuffer && rb.binding == 0)
                                    {
                                        instanceUBO = &rb;
                                        break;
                                    }
                                }
                            }

                            if (instanceUBO && !instanceUBO->members.empty())
                            {
                                ImGui::SeparatorText("Uniforms");

                                // Sort members by offset for stable display order.
                                std::vector<const ReflectedMember*> sortedMembers;
                                sortedMembers.reserve(instanceUBO->members.size());
                                for (const auto& m : instanceUBO->members)
                                    sortedMembers.push_back(&m);
                                std::sort(sortedMembers.begin(), sortedMembers.end(),
                                    [](const ReflectedMember* a, const ReflectedMember* b) {
                                        return a->offset < b->offset; });

                                for (const ReflectedMember* member : sortedMembers)
                                {
                                    // Ensure the material has a value for this member.
                                    auto [it, inserted] = mat.material->uniformValues.try_emplace(
                                        member->name,
                                        UniformValue{ DataTypeToUniformValueType(member->type), glm::vec4(1.0f) });
                                    UniformValue& uv = it->second;

                                    // Heuristic: members with "color" in their name use a color picker.
                                    const bool isColor =
                                        (member->name.find("color") != std::string::npos ||
                                         member->name.find("Color") != std::string::npos);

                                    ImGui::PushID(member->name.c_str());
                                    switch (member->type)
                                    {
                                        case DataType::Float:
                                            ImGui::DragFloat(member->name.c_str(), &uv.data.x, 0.01f);
                                            break;
                                        case DataType::Vec2:
                                            ImGui::DragFloat2(member->name.c_str(), &uv.data.x, 0.01f);
                                            break;
                                        case DataType::Vec3:
                                            if (isColor)
                                                ImGui::ColorEdit3(member->name.c_str(), &uv.data.x);
                                            else
                                                ImGui::DragFloat3(member->name.c_str(), &uv.data.x, 0.01f);
                                            break;
                                        case DataType::Vec4:
                                            if (isColor)
                                                ImGui::ColorEdit4(member->name.c_str(), &uv.data.x);
                                            else
                                                ImGui::DragFloat4(member->name.c_str(), &uv.data.x, 0.01f);
                                            break;
                                        case DataType::Int:
                                        {
                                            int v = static_cast<int>(uv.data.x);
                                            if (ImGui::DragInt(member->name.c_str(), &v))
                                                uv.data.x = static_cast<float>(v);
                                            break;
                                        }
                                        case DataType::IVec2:
                                        {
                                            int v[2] = { static_cast<int>(uv.data.x),
                                                         static_cast<int>(uv.data.y) };
                                            if (ImGui::DragInt2(member->name.c_str(), v))
                                            {
                                                uv.data.x = static_cast<float>(v[0]);
                                                uv.data.y = static_cast<float>(v[1]);
                                            }
                                            break;
                                        }
                                        case DataType::IVec3:
                                        {
                                            int v[3] = { static_cast<int>(uv.data.x),
                                                         static_cast<int>(uv.data.y),
                                                         static_cast<int>(uv.data.z) };
                                            if (ImGui::DragInt3(member->name.c_str(), v))
                                            {
                                                uv.data.x = static_cast<float>(v[0]);
                                                uv.data.y = static_cast<float>(v[1]);
                                                uv.data.z = static_cast<float>(v[2]);
                                            }
                                            break;
                                        }
                                        case DataType::IVec4:
                                        {
                                            int v[4] = { static_cast<int>(uv.data.x),
                                                         static_cast<int>(uv.data.y),
                                                         static_cast<int>(uv.data.z),
                                                         static_cast<int>(uv.data.w) };
                                            if (ImGui::DragInt4(member->name.c_str(), v))
                                            {
                                                uv.data.x = static_cast<float>(v[0]);
                                                uv.data.y = static_cast<float>(v[1]);
                                                uv.data.z = static_cast<float>(v[2]);
                                                uv.data.w = static_cast<float>(v[3]);
                                            }
                                            break;
                                        }
                                        default:
                                            ImGui::TextDisabled("%s (unsupported type)", member->name.c_str());
                                            break;
                                    }
                                    ImGui::PopID();
                                }
                            }
                        }

                        ImGui::SeparatorText("Texture Maps");

                        if (!effectiveShader || effectiveShader->GetState() != ResourceState::GPU_READY)
                        {
                            ImGui::TextDisabled("Shader not ready.");
                        }
                        else
                        {
                            const auto& setIt = effectiveShader->reflection.descriptorSets.find(1);
                            bool hasSamplers = (setIt != effectiveShader->reflection.descriptorSets.end());
                            if (hasSamplers)
                            {
                                // Collect and sort samplers by binding index for stable display order.
                                std::vector<const ReflectedBinding*> samplers;
                                for (const auto& rb : setIt->second)
                                    if (rb.type == DescriptorType::CombinedImageSampler)
                                        samplers.push_back(&rb);
                                std::sort(samplers.begin(), samplers.end(),
                                    [](const ReflectedBinding* a, const ReflectedBinding* b){
                                        return a->binding < b->binding; });

                                hasSamplers = !samplers.empty();

                                for (const ReflectedBinding* rb : samplers)
                                {
                                    ImGui::PushID(static_cast<int>(rb->binding));

                                    // Current texture for this slot (may be null)
                                    ResourceTexture* currentTex = nullptr;
                                    auto texIt = mat.material->textureMaps.find(rb->name);
                                    if (texIt != mat.material->textureMaps.end())
                                        currentTex = texIt->second.texture;

                                    // Label + drop button
                                    ImGui::TextUnformatted((rb->name + ":").c_str());
                                    ImGui::SameLine();
                                    const std::string btnLabel = currentTex ? currentTex->GetName() : "(none — drop here)";
                                    ImGui::Button(btnLabel.c_str(), ImVec2(200.0f, 0.0f));

                                    if (ImGui::BeginDragDropTarget())
                                    {
                                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
                                        {
                                            // Take the first path in the multi-item payload
                                            std::string droppedPath(static_cast<const char*>(payload->Data));
                                            std::replace(droppedPath.begin(), droppedPath.end(), '\\', '/');

                                            // Validate texture extension
                                            const size_t dotPos = droppedPath.find_last_of('.');
                                            std::string ext = (dotPos != std::string::npos) ? droppedPath.substr(dotPos) : "";
                                            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                                            static const char* s_texExts[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
                                            bool isTexture = false;
                                            for (const char* e : s_texExts)
                                                if (ext == e) { isTexture = true; break; }

                                            if (isTexture)
                                            {
                                                Resource* r = rm->CreateResource(droppedPath);
                                                if (r)
                                                {
                                                    if (currentTex)
                                                        rm->UnloadResource(currentTex->GetUID());
                                                    mat.material->textureMaps[rb->name].texture =
                                                        down_cast<ResourceTexture*>(r);
                                                }
                                                else
                                                {
                                                    NOUS_ERROR("InspectorWindow — failed to load texture '%s'.", droppedPath.c_str());
                                                }
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }

                                    // [x] clear button — only shown when a texture is assigned
                                    if (currentTex)
                                    {
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton("x"))
                                        {
                                            rm->UnloadResource(currentTex->GetUID());
                                            mat.material->textureMaps[rb->name].texture = nullptr;
                                        }
                                    }

                                    ImGui::PopID();
                                }
                            }

                            if (!hasSamplers)
                                ImGui::TextDisabled("No texture slots in this shader.");
                        }

                        if (ImGui::Button("Save Material"))
                            ImporterMaterial::SaveMaterialToAssets(mat.material);

                        } // end else (non-default material)
                    }
                }
            }

            // --- Script Component ---
            if (go->HasComponent<CScript>()) {
                auto& cs = go->GetComponent<CScript>();
                const auto& scriptNames = cs.GetScriptNames();

                if (ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent();

                    // One sub-section per attached script
                    for (int i = 0; i < static_cast<int>(scriptNames.size()); ++i)
                    {
                        const std::string& sName = scriptNames[i];

                        // Script header with remove button.
                        // Render the Remove button first (right-aligned) so it gets
                        // click priority over the CollapsingHeader that spans the row.
                        ImGui::PushID(i);
                        float removeWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                        float availWidth  = ImGui::GetContentRegionAvail().x;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - removeWidth);
                        if (ImGui::SmallButton("Remove")) {
                            cs.RemoveScript(sName);
                            ImGui::PopID();
                            break; // iterator invalidated — skip rest of frame
                        }
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - availWidth);
                        bool open = ImGui::CollapsingHeader(sName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

                        // Properties (if script has any)
                        if (open) {
                            IScript* inst = cs.GetInstance(i);
                            if (inst) {
                                ImGui::Indent();
                                auto props = inst->GetProperties();
                                if (props.empty()) {
                                    ImGui::TextDisabled("No exposed properties.");
                                }
                                for (auto& prop : props) {
                                    switch (prop.type) {
                                        case ScriptProperty::Type::Float:
                                            ImGui::DragFloat(prop.name,
                                                             static_cast<float*>(prop.ptr),
                                                             0.1f);
                                            break;
                                        case ScriptProperty::Type::Int:
                                            ImGui::DragInt(prop.name,
                                                           static_cast<int*>(prop.ptr));
                                            break;
                                        case ScriptProperty::Type::Bool:
                                            ImGui::Checkbox(prop.name,
                                                            static_cast<bool*>(prop.ptr));
                                            break;
                                        case ScriptProperty::Type::GameObject: {
                                            auto* idPtr = static_cast<uint32_t*>(prop.ptr);

                                            // Resolve current name for the preview label
                                            std::string preview = "None";
                                            if (*idPtr != 0 && editorContext->GetScene()->activeScene) {
                                                auto* target = editorContext->GetScene()->activeScene->GetGameObjectByID(*idPtr);
                                                preview = target ? target->GetName() : "(missing)";
                                            }

                                            if (ImGui::BeginCombo(prop.name, preview.c_str())) {
                                                // None option
                                                if (ImGui::Selectable("None", *idPtr == 0))
                                                    *idPtr = 0;

                                                // All scene GameObjects
                                                if (editorContext->GetScene()->activeScene) {
                                                    const auto gos = editorContext->GetScene()->activeScene->GetGameObjectsSnapshot();
                                                    for (auto* target : gos) {
                                                        const bool selected = *idPtr == target->GetID();
                                                        if (ImGui::Selectable(target->GetName().c_str(), selected))
                                                            *idPtr = target->GetID();
                                                        if (selected) ImGui::SetItemDefaultFocus();
                                                    }
                                                }
                                                ImGui::EndCombo();
                                            }
                                            break;
                                        }
                                    }
                                }
                                ImGui::Unindent();
                            }
                        }

                        ImGui::PopID();
                    }

                    // Add script from dropdown
                    ImGui::Spacing();
                    const std::vector<std::string> available =
                        editorContext->GetScene()->scriptManager->GetAvailableScriptNames();

                    if (!available.empty()) {
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
                            cs.AddScript(available[s_selectedScript]);
                    } else {
                        ImGui::TextDisabled("No scripts in registry (rebuild DLL?)");
                    }

                    ImGui::Unindent();
                }
            }

            // --- Add Component section ---
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (!go->HasComponent<CScript>()) {
                if (ImGui::Button("Add CScript Component"))
                    go->AddComponent<CScript>();
            }

        }
        ImGui::End();
    }
}

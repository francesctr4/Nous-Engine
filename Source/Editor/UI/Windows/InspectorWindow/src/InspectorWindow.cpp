#include "Editor/UI/Windows/InspectorWindow/include/InspectorWindow.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Scripting/ScriptManager.h"
#include "Engine/Scripting/Internal/IScript.inl"

#include "imgui.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "Engine/Systems/ECS/Scene/include/Scene.h"

InspectorWindow::InspectorWindow(const char* title, EditorContext* context, bool start_open)
        : IEditorWindow(title, context, nullptr, start_open) {
    Init();
}

void InspectorWindow::Init()
{

}

void InspectorWindow::Draw() {
    if (*p_open) {
        if (ImGui::Begin(title, p_open)) {

            GameObject* go = External->scene->selectedGameObject;
            if (!go) {
                ImGui::TextDisabled("No GameObject selected.");
                ImGui::End();
                return;
            }

            // --- GameObject Header ---
            ImGui::SeparatorText("GameObject Info");
            static char buffer[256];
            strncpy(buffer, go->GetName().c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
                go->SetName(buffer);
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
                    auto& mesh = go->GetComponent<CMesh>();
                    if (mesh.mesh) {
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

            // --- Material Component ---
            if (go->HasComponent<CMaterial>()) {
                if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& mat = go->GetComponent<CMaterial>();
                    if (mat.material) {
                        ImGui::Text("Name: %s", mat.material->GetName().c_str());
                        ImGui::Text("UID: %u", mat.material->GetUID());
                        ImGui::Text("Assets Path: %s", mat.material->GetAssetsPath().c_str());
                        ImGui::Text("Library Path: %s", mat.material->GetLibraryPath().c_str());

                        ImGui::SeparatorText("Texture Maps");
                        if (mat.material->diffuseMap.texture) {
                            ImGui::Text("Diffuse: %s", mat.material->diffuseMap.texture->GetName().c_str());
                        } else {
                            ImGui::TextDisabled("No diffuse texture assigned.");
                        }
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

                        // Script header with remove button
                        ImGui::PushID(i);
                        bool open = ImGui::CollapsingHeader(sName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove")) {
                            cs.RemoveScript(sName);
                            ImGui::PopID();
                            break; // iterator invalidated — skip rest of frame
                        }

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
                                            if (*idPtr != 0 && External->scene->activeScene) {
                                                auto* target = External->scene->activeScene->GetGameObjectByID(*idPtr);
                                                preview = target ? target->GetName() : "(missing)";
                                            }

                                            if (ImGui::BeginCombo(prop.name, preview.c_str())) {
                                                // None option
                                                if (ImGui::Selectable("None", *idPtr == 0))
                                                    *idPtr = 0;

                                                // All scene GameObjects
                                                if (External->scene->activeScene) {
                                                    const auto gos = External->scene->activeScene->GetGameObjectsSnapshot();
                                                    for (auto* target : gos) {
                                                        const bool selected = (*idPtr == target->GetID());
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
                        External->scene->scriptManager->GetAvailableScriptNames();

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

#include "Editor/UI/Windows/InspectorWindow/include/InspectorWindow.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"

#include "imgui.h"

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
            strncpy_s(buffer, go->GetName().c_str(), sizeof(buffer));
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

                    // Rotation
                    if (ImGui::DragFloat3("Rotation (Euler)", &transform.rotation.x, 0.1f))
                        transform.UpdateMatrix();

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

        }
        ImGui::End();
    }
}

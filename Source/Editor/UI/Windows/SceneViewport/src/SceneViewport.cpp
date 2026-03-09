#include "Editor/UI/Windows/SceneViewport/include/SceneViewport.h"

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"

#include "Engine/Core/Application.h"

#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_vulkan.h"

#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SDL3/SDL.h"

SceneViewport::SceneViewport(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    Init();
}

void SceneViewport::Init()
{
    CreateSceneViewportDescriptorSets();
}

void SceneViewport::Draw()
{
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            External->camera->sceneViewportHovered = ImGui::IsWindowHovered();

            // Handle gizmo mode switching (W/E/R keys) when viewport is hovered
            if (ImGui::IsWindowHovered() || ImGui::IsWindowFocused())
            {
                HandleGizmoInput();
            }

            // Get the size of the window's content area
            ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
            ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 squareSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
            ImVec2 squarePos = ImVec2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
            ImVec2 squareEnd = ImVec2(squarePos.x + squareSize.x, squarePos.y + squareSize.y);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Draw gray background
            drawList->AddRectFilled(squarePos, squareEnd, IM_COL32(100, 100, 100, 255));

            // Calculate aspect ratios and UV coordinates
            float textureWidth = 1920.0f;
            float textureHeight = 1080.0f;

            float textureAspect = textureWidth / textureHeight;
            float viewportAspect = squareSize.x / squareSize.y;

            ImVec2 uvMin(0.0f, 0.0f);
            ImVec2 uvMax(1.0f, 1.0f);

            if (viewportAspect < textureAspect)
            {
                // Viewport is narrower: crop left/right
                float cropFactor = textureAspect / viewportAspect;
                uvMin.x = 0.5f - 0.5f / cropFactor;
                uvMax.x = 0.5f + 0.5f / cropFactor;
            }
            else if (viewportAspect > textureAspect)
            {
                // Viewport is wider: crop top/bottom
                float cropFactor = viewportAspect / textureAspect;
                uvMin.y = 0.5f - 0.5f / cropFactor;
                uvMax.y = 0.5f + 0.5f / cropFactor;
            }

            // Position the image at the start of the content region and render
            ImGui::SetCursorPos(contentMin); // Position relative to window's content area

            VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

            if (squareSize.x > 0.0f && squareSize.y > 0.0f)
            {
                ImGui::Image(
                        static_cast<ImTextureID>(
                                NOUS_ImGuiVulkanResources::GetViewportTexture(
                                        vkContext->imGuiResources.m_ViewportDescriptorSets[vkContext->imageIndex])),
                        squareSize, uvMin, uvMax);
            }

            // Draw white border on top
            drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));

            // Invisible button for drag-and-drop target area
            ImGui::SetCursorScreenPos(squarePos);
            ImGui::InvisibleButton("DropTarget", squareSize);

            // Draw the gizmo on top — must be called after InvisibleButton so it can
            // override the active widget when the user clicks on a gizmo axis.
            // ImGuizmo::IsOver() returns true when the mouse is over the gizmo,
            // allowing us to prevent the InvisibleButton from stealing future clicks.
            DrawGizmo(squarePos, squareSize);

            // If gizmo is being interacted with, clear ImGui's active ID so the
            // InvisibleButton doesn't hold mouse capture and block the gizmo
            if (ImGuizmo::IsUsing())
            {
                ImGui::ClearActiveID();
            }

            // Start the drag-and-drop target
            if (ImGui::BeginDragDropTarget())
            {
                // Accept the payload
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

                if (payload != NULL)
                {
                    const char* payload_data = (const char*)payload->Data;
                    std::vector<std::string> filePaths;

                    // Split the payload data into individual file paths (e.g., null-terminated strings)
                    while (*payload_data)
                    {
                        std::string path(payload_data);

                        // Validate the path to ensure it contains only valid ASCII characters
                        if (!path.empty() && IsValidASCII(path))
                        {
                            filePaths.push_back(path);
                        }

                        // Move to the next string (skip the null terminator)
                        payload_data += path.length() + 1;
                    }

                    // Process each file path
                    for (const auto& path : filePaths)
                    {
                        ImGui::Text("Dropped file: %s", path.c_str());

                        if (path.find(".nous") != std::string::npos)
                        {
                            External->scene->LoadScene(path);
                            continue;
                        }

                        External->jobSystem->SubmitJob([path]()
                            {
                                External->resourceManager->CreateResource(path);
                            }, "Create Resource");

                    }

                }

                // End the drag-and-drop target
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::End();
    }
}

void SceneViewport::HandleGizmoInput()
{
    // Only switch gizmo mode when right mouse button is NOT held (camera uses RMB + WASD)
    if (External->input->GetMouseButton(SDL_BUTTON_RIGHT) == KeyState::IDLE)
    {
        if (External->input->GetKey(SDL_SCANCODE_W) == KeyState::DOWN)
            m_GizmoOperation = GizmoOperation::TRANSLATE;
        if (External->input->GetKey(SDL_SCANCODE_E) == KeyState::DOWN)
            m_GizmoOperation = GizmoOperation::ROTATE;
        if (External->input->GetKey(SDL_SCANCODE_R) == KeyState::DOWN)
            m_GizmoOperation = GizmoOperation::SCALE;
    }

    // Toggle local/world mode
    if (External->input->GetKey(SDL_SCANCODE_X) == KeyState::DOWN)
    {
        m_GizmoSpace = (m_GizmoSpace == GizmoSpace::LOCAL)
            ? GizmoSpace::WORLD
            : GizmoSpace::LOCAL;
    }

    // Toggle snapping with Left Ctrl
    m_UseSnap = (External->input->GetKey(SDL_SCANCODE_LCTRL) == KeyState::REPEAT
              || External->input->GetKey(SDL_SCANCODE_LCTRL) == KeyState::DOWN);
}

void SceneViewport::DrawGizmo(const ImVec2& viewportPos, const ImVec2& viewportSize)
{
    GameObject* selected = External->scene->selectedGameObject;
    if (!selected || !selected->HasComponent<CTransform>())
        return;

    Camera* cam = External->camera->GetCamera();
    if (!cam)
        return;

    // Get camera matrices — ImGuizmo renders via ImGui draw lists (not Vulkan),
    // so it expects standard OpenGL-convention matrices. glm::perspective already produces that.
    glm::mat4 view = cam->GetViewMatrix();

    // Build a projection matrix that matches the viewport panel's aspect ratio.
    // The scene image is UV-cropped to fit the panel, so the gizmo projection must
    // use the panel's aspect ratio to stay aligned with what the user sees.
    float viewportAspect = viewportSize.x / viewportSize.y;
    glm::mat4 projection = glm::perspective(
        glm::radians(cam->GetVerticalFOV()),
        viewportAspect,
        cam->GetNearPlane(),
        cam->GetFarPlane()
    );

    // Get the object's transform matrix
    CTransform& transform = selected->GetComponent<CTransform>();
    glm::mat4 objectMatrix = transform.GetLocalMatrix();

    // Configure ImGuizmo for this viewport
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

    // Map our enum to ImGuizmo operation
    ImGuizmo::OPERATION operation;
    switch (m_GizmoOperation)
    {
        case GizmoOperation::TRANSLATE: operation = ImGuizmo::OPERATION::TRANSLATE; break;
        case GizmoOperation::ROTATE:    operation = ImGuizmo::OPERATION::ROTATE;    break;
        case GizmoOperation::SCALE:     operation = ImGuizmo::OPERATION::SCALE;     break;
        default:                        operation = ImGuizmo::OPERATION::TRANSLATE;  break;
    }

    // Set snap values based on current operation
    float snapValues[3] = { 0.0f, 0.0f, 0.0f };
    if (m_UseSnap)
    {
        switch (m_GizmoOperation)
        {
            case GizmoOperation::TRANSLATE:
                snapValues[0] = snapValues[1] = snapValues[2] = m_TranslateSnap;
                break;
            case GizmoOperation::ROTATE:
                snapValues[0] = snapValues[1] = snapValues[2] = m_RotateSnap;
                break;
            case GizmoOperation::SCALE:
                snapValues[0] = snapValues[1] = snapValues[2] = m_ScaleSnap;
                break;
        }
    }

    // Scale mode must use LOCAL space (world scale doesn't make sense)
    ImGuizmo::MODE mode = (m_GizmoOperation == GizmoOperation::SCALE)
        ? ImGuizmo::LOCAL
        : static_cast<ImGuizmo::MODE>(m_GizmoSpace);

    // Render and manipulate the gizmo
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        operation,
        mode,
        glm::value_ptr(objectMatrix),
        nullptr,
        m_UseSnap ? snapValues : nullptr
    );

    // If the gizmo was manipulated, decompose the matrix back into transform components
    if (ImGuizmo::IsUsing())
    {
        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(objectMatrix),
            matrixTranslation,
            matrixRotation,
            matrixScale
        );

        transform.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
        transform.rotation = glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
        transform.scale    = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);

        transform.UpdateMatrix();
    }
}

// Utility function to check if a string contains only valid ASCII characters.
bool SceneViewport::IsValidASCII(const std::string& str)
{
    for (auto c : str)
    {
        if (static_cast<unsigned char>(c) > 127)
        {
            return false;
        }
    }
    return true;
}

void SceneViewport::CreateSceneViewportDescriptorSets()
{
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
    {
        vkContext->imGuiResources.m_ViewportDescriptorSets[i] = ImGui_ImplVulkan_AddTexture(
                vkContext->imGuiResources.m_ViewportTextureSampler,
                vkContext->imGuiResources.m_ViewportImages[i].view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void SceneViewport::DestroySceneViewportDescriptorSets()
{
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    for (uint32 i = 0; i < vkContext->imGuiResources.m_ViewportImages.size(); ++i)
    {
        ImGui_ImplVulkan_RemoveTexture(vkContext->imGuiResources.m_ViewportDescriptorSets[i]);
    }
}

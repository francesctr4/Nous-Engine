#include "Editor/UI/Windows/SceneViewport/include/SceneViewport.h"

#include <algorithm>   // std::clamp

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"

#include "Engine/Core/Application.h"

#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/RendererTypes.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_vulkan.h"

#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

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
        // Prevent a floating window from being moved by ImGui while the gizmo is active.
        // Window movement is processed inside Begin(), so we must pass NoMove *to* Begin()
        // using the gizmo state from the previous frame (static bool persists across calls).
        static bool s_GizmoWasActive = false;
        const ImGuiWindowFlags windowFlags = s_GizmoWasActive ? ImGuiWindowFlags_NoMove
                                                               : ImGuiWindowFlags_None;

        if (ImGui::Begin(title, p_open, windowFlags))
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

            if (squareSize.x > 0.0f && squareSize.y > 0.0f)
            {
                // Calculate aspect ratios and UV coordinates
                VulkanContext* vkCtx = VulkanBackend::GetVulkanContext();
                float textureWidth  = static_cast<float>(vkCtx->framebufferWidth);
                float textureHeight = static_cast<float>(vkCtx->framebufferHeight);

                float textureAspect  = textureWidth / textureHeight;
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
                ImGui::SetCursorPos(contentMin);
                ImGui::Image(
                        static_cast<ImTextureID>(
                                NOUS_ImGuiVulkanResources::GetViewportTexture(
                                        vkCtx->imGuiResources.m_ViewportDescriptorSets[vkCtx->imageIndex])),
                        squareSize, uvMin, uvMax);

                // Draw white border on top
                drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));

                // Draw the gizmo on top of the scene image
                DrawGizmo(squarePos, squareSize);

                // ImGuizmo::IsOver() retains stale state from the previous frame when no
                // gizmo was drawn (e.g. nothing selected). Only consult it when a gizmo is
                // actually visible this frame, otherwise picking would be incorrectly blocked.
                const bool gizmoVisible = External->scene->selectedGameObject != nullptr &&
                                          External->scene->selectedGameObject->HasComponent<CTransform>();
                const bool gizmoBlocking = gizmoVisible && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
                s_GizmoWasActive = gizmoBlocking;

                // Handle mouse picking (click to select/deselect objects)
                if (!gizmoBlocking)
                {
                    HandleMousePicking(squarePos, squareSize, uvMin, uvMax);
                }

                // Drag-and-drop target — only place the InvisibleButton when the gizmo
                // is not being hovered/used, so it doesn't steal mouse input from ImGuizmo
                if (!gizmoBlocking)
                {
                    ImGui::SetCursorScreenPos(squarePos);
                    ImGui::InvisibleButton("DropTarget", squareSize);

                    if (ImGui::BeginDragDropTarget())
                    {
                        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

                        if (payload != NULL)
                        {
                            const char* payload_data = (const char*)payload->Data;
                            std::vector<std::string> filePaths;

                            while (*payload_data)
                            {
                                std::string path(payload_data);

                                if (!path.empty() && IsValidASCII(path))
                                {
                                    filePaths.push_back(path);
                                }

                                payload_data += path.length() + 1;
                            }

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

                        ImGui::EndDragDropTarget();
                    }
                }
            }
            else
            {
                s_GizmoWasActive = false;
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
    if (External->input->GetKey(SDL_SCANCODE_T) == KeyState::DOWN)
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
    // SetImGuiContext is critical in DLL architectures where ImGui and ImGuizmo
    // may not share the same global context
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::Enable(true);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
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

    // If the gizmo was manipulated, decompose the matrix using GLM (quaternion-based)
    if (ImGuizmo::IsUsing())
    {
        glm::vec3 newPosition, newScale, skew;
        glm::vec4 perspective;
        glm::quat newOrientation;
        glm::decompose(objectMatrix, newScale, newOrientation, newPosition, skew, perspective);

        transform.position = newPosition;
        transform.orientation = newOrientation;
        transform.scale = newScale;
        transform.eulerHint = transform.GetEulerAngles();

        transform.UpdateMatrix();
    }
}

void SceneViewport::HandleMousePicking(const ImVec2& viewportPos, const ImVec2& viewportSize,
                                       const ImVec2& uvMin, const ImVec2& uvMax)
{
    // Only respond to left mouse button click when the viewport is hovered and
    // right mouse button is NOT held (camera uses RMB + WASD).
    if (!ImGui::IsWindowHovered() ||
        External->input->GetMouseButton(SDL_BUTTON_LEFT) != KeyState::DOWN ||
        External->input->GetMouseButton(SDL_BUTTON_RIGHT) != KeyState::IDLE)
    {
        return;
    }

    // Get mouse position relative to the viewport panel
    ImVec2 mousePos = ImGui::GetMousePos();
    float relX = (mousePos.x - viewportPos.x) / viewportSize.x;
    float relY = (mousePos.y - viewportPos.y) / viewportSize.y;

    // Check bounds
    if (relX < 0.0f || relX > 1.0f || relY < 0.0f || relY > 1.0f)
        return;

    // Map from panel-relative [0,1] to UV coordinates (accounting for cropping)
    float uvX = uvMin.x + relX * (uvMax.x - uvMin.x);
    float uvY = uvMin.y + relY * (uvMax.y - uvMin.y);

    // Map UV to framebuffer pixel coordinates (round to nearest, clamp to valid range)
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();
    int32_t pixelX = std::clamp(static_cast<int32_t>(uvX * static_cast<float>(vkContext->framebufferWidth)  + 0.5f),
                                0, static_cast<int32_t>(vkContext->framebufferWidth  - 1));
    int32_t pixelY = std::clamp(static_cast<int32_t>(uvY * static_cast<float>(vkContext->framebufferHeight) + 0.5f),
                                0, static_cast<int32_t>(vkContext->framebufferHeight - 1));

    // Build geometry list (same logic as ModuleRenderer3D::BuildRenderPacket)
    if (!External->scene->activeScene)
        return;

    const auto gameObjects = External->scene->activeScene->GetGameObjectsSnapshot();
    std::vector<GeometryRenderData> geometries;
    geometries.reserve(gameObjects.size());

    for (const auto& goPtr : gameObjects)
    {
        if (!goPtr->HasComponent<CMesh>()) continue;

        GeometryRenderData data{};
        data.objectUID = goPtr->GetID();

        if (auto* transform = goPtr->TryGetComponent<CTransform>())
            data.model = transform->worldMatrix;

        if (auto* mesh = goPtr->TryGetComponent<CMesh>())
            data.geometry = mesh->mesh;

        geometries.emplace_back(data);
    }

    if (geometries.empty())
    {
        External->scene->selectedGameObject = nullptr;
        return;
    }

    // Use the camera's own projection matrix — must match what the scene pass renders
    // with, so the pick result aligns with the scene framebuffer.
    // The UV mapping above already accounts for the viewport panel's crop.
    Camera* cam = External->camera->GetCamera();
    if (!cam) return;

    glm::mat4 projection = cam->GetProjectionMatrix();
    glm::mat4 view = cam->GetViewMatrix();

    // Perform the pick
    RendererFrontend* frontend = External->renderer->GetRendererFrontend();
    uint32_t objectID = frontend->PickObjectAt(pixelX, pixelY, projection, view, geometries);

    // Select or deselect
    if (objectID != 0)
    {
        GameObject* found = External->scene->activeScene->FindGameObjectByID(objectID);
        if (found)
        {
            External->scene->selectedGameObject = found;
        }
        else
        {
            // objectID didn't match any live GO — stale pick result; deselect.
            External->scene->selectedGameObject = nullptr;
        }
    }
    else
    {
        External->scene->selectedGameObject = nullptr;
    }
}

// Utility function to check if a string contains only valid ASCII characters.
bool SceneViewport::IsValidASCII(const std::string& str)
{
    for (const auto c : str)
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

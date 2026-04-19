#include "Editor/UI/Windows/SceneViewport/include/SceneViewport.h"

#include <algorithm>   // std::clamp

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"

#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
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
#include <utility>

#include "SDL3/SDL.h"

SceneViewport::SceneViewport(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    SceneViewport::Init();
}

void SceneViewport::Init()
{
    CreateSceneViewportDescriptorSets();
}

void SceneViewport::Draw()
{
    if (!*p_open) return;

    // Prevent a floating window from being moved by ImGui while the gizmo is active.
    // Window movement is processed inside Begin(), so we must pass NoMove *to* Begin()
    // using the gizmo state from the previous frame (static bool persists across calls).
    static bool s_GizmoWasActive = false;
    if (!ImGui::Begin(title, p_open,
                      s_GizmoWasActive ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    editorContext->GetCamera()->sceneViewportHovered = ImGui::IsWindowHovered();

    // Handle gizmo mode switching (W/E/R keys) when viewport is hovered
    if (ImGui::IsWindowHovered() || ImGui::IsWindowFocused())
    {
        HandleGizmoInput();
    }

    // Get the size of the window's content area
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImVec2 windowPos = ImGui::GetWindowPos();
    auto squareSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
    auto squarePos = ImVec2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
    auto squareEnd = ImVec2(squarePos.x + squareSize.x, squarePos.y + squareSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw gray background
    drawList->AddRectFilled(squarePos, squareEnd, IM_COL32(100, 100, 100, 255));

    if (squareSize.x <= 0.0f || squareSize.y <= 0.0f)
    {
        s_GizmoWasActive = false;
        ImGui::End();
        return;
    }

    // Drive camera aspect ratio from panel size — Hor+ scaling (vertical FOV fixed).
    // This keeps scene rendering, ImGuizmo, and mouse picking all consistent.
    if (Camera* editorCam = editorContext->GetCamera()->GetCamera())
        editorCam->SetAspectRatio(squareSize.x / squareSize.y);

    VulkanContext* vkCtx = VulkanBackend::GetVulkanContext();
    constexpr ImVec2 uvMin(0.0f, 0.0f);
    constexpr ImVec2 uvMax(1.0f, 1.0f);

    // Position the image at the start of the content region and render
    ImGui::SetCursorPos(contentMin);
    ImGui::Image(
        NOUS_ImGuiVulkanResources::GetViewportTexture(
            vkCtx->imGuiResources.m_ViewportDescriptorSets[vkCtx->imageIndex]),
        squareSize, uvMin, uvMax);

    // Draw white border on top
    drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));

    // Draw the gizmo on top of the scene image
    DrawGizmo(squarePos, squareSize);

    // ImGuizmo::IsOver() retains stale state from the previous frame when no
    // gizmo was drawn (e.g. nothing selected). Only consult it when a gizmo is
    // actually visible this frame, otherwise picking would be incorrectly blocked.
    const bool gizmoVisible = editorContext->GetScene()->selectedGameObject.IsValid() &&
        editorContext->GetScene()->selectedGameObject.HasComponent<CTransform>();
    const bool gizmoBlocking = gizmoVisible && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());

    // Keep the camera orbit target in sync with the selected object's world position.
    ModuleCamera3D* cam3D = editorContext->GetCamera();
    if (gizmoVisible)
    {
        const CTransform& t = editorContext->GetScene()->selectedGameObject.GetComponent<CTransform>();
        cam3D->SetOrbitTarget(glm::vec3(t.worldMatrix[3]));
    }
    else
    {
        cam3D->ClearOrbitTarget();
    }
    s_GizmoWasActive = gizmoBlocking;

    // Handle mouse picking (click to select/deselect objects)
    if (!gizmoBlocking)
    {
        HandleMousePicking(squarePos, squareSize);
    }

    // Drag-and-drop target — only place the InvisibleButton when the gizmo
    // is not being hovered/used, so it doesn't steal mouse input from ImGuizmo

    if (gizmoBlocking)
    {
        ImGui::End();
        return;
    }

    ImGui::SetCursorScreenPos(squarePos);
    ImGui::InvisibleButton("DropTarget", squareSize);

    if (!ImGui::BeginDragDropTarget())
    {
        ImGui::End();
        return;
    }

    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

    if (payload == nullptr)
    {
        ImGui::EndDragDropTarget();
        ImGui::End();
        return;
    }

    const auto* payload_data = (const char*)payload->Data;
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

        if (path.contains(".nous"))
        {
            editorContext->GetScene()->LoadScene(path);
            continue;
        }

        // Mesh assets → spawn a full submesh hierarchy.
        // Prefab assets → instantiate prefab into scene.
        // All other assets → just load the resource.
        const std::string ext = NOUS_FileManager::GetExtension(path);
        if (Resource::GetTypeFromExtension(ext) == ResourceType::MESH)
        {
            editorContext->GetJobSystem()->SubmitJob([path, this]()
            {
                editorContext->GetScene()->SpawnMeshAsHierarchy(path);
            }, "Spawn Mesh Hierarchy");
        }
        else if (ext == ".nprefab")
        {
            editorContext->GetScene()->InstantiatePrefab(path);
        }
        else
        {
            editorContext->GetJobSystem()->SubmitJob([path, this]()
            {
                editorContext->GetResourceManager()->CreateResource(path);
            }, "Create Resource");
        }
    }

    ImGui::EndDragDropTarget();

    ImGui::End();
}

void SceneViewport::HandleGizmoInput()
{
    using enum KeyState;

    // Only switch gizmo mode when right mouse button is NOT held (camera uses RMB + WASD)
    if (editorContext->GetInput()->GetMouseButton(SDL_BUTTON_RIGHT) == IDLE)
    {
        using enum GizmoOperation;

        if (editorContext->GetInput()->GetKey(SDL_SCANCODE_W) == DOWN)
            m_GizmoOperation = TRANSLATE;
        if (editorContext->GetInput()->GetKey(SDL_SCANCODE_E) == DOWN)
            m_GizmoOperation = ROTATE;
        if (editorContext->GetInput()->GetKey(SDL_SCANCODE_R) == DOWN)
            m_GizmoOperation = SCALE;
    }

    // Toggle local/world mode
    if (editorContext->GetInput()->GetKey(SDL_SCANCODE_T) == DOWN)
    {
        using enum GizmoSpace;

        m_GizmoSpace = m_GizmoSpace == LOCAL ? WORLD : LOCAL;
    }

    // Frame selected object (F) — move camera to look at the selected object
    if (editorContext->GetInput()->GetKey(SDL_SCANCODE_F) == DOWN)
    {
        GameObject selected = editorContext->GetScene()->selectedGameObject;
        if (selected.IsValid() && selected.HasComponent<CTransform>())
        {
            const CTransform& t = selected.GetComponent<CTransform>();
            const auto worldPos = glm::vec3(t.worldMatrix[3]);
            const float maxScale = glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z));
            const float distance = glm::max(maxScale * 500.0f, 500.0f);
            editorContext->GetCamera()->FrameTarget(worldPos, distance);
        }
    }

    // Toggle snapping with Left Ctrl
    m_UseSnap = (editorContext->GetInput()->GetKey(SDL_SCANCODE_LCTRL) == REPEAT
        || editorContext->GetInput()->GetKey(SDL_SCANCODE_LCTRL) == DOWN);
}

void SceneViewport::DrawGizmo(const ImVec2& viewportPos, const ImVec2& viewportSize) const
{
    GameObject selected = editorContext->GetScene()->selectedGameObject;
    if (!selected.IsValid() || !selected.HasComponent<CTransform>())
        return;

    Camera const* cam = editorContext->GetCamera()->GetCamera();
    if (!cam)
        return;

    // Get camera matrices — ImGuizmo renders via ImGui draw lists (not Vulkan),
    // so it expects standard OpenGL-convention matrices. glm::perspective already produces that.
    glm::mat4 view = cam->GetViewMatrix();

    // Camera aspect ratio is driven from the panel each frame, so the camera's own
    // projection matrix is already correct — no need to rebuild it here.
    glm::mat4 projection = cam->GetProjectionMatrix();

    // Get the object's transform matrix.
    // Use worldMatrix so the gizmo is placed at the correct world position even
    // when the selected object is a child of another GO.
    CTransform& transform = selected.GetComponent<CTransform>();
    glm::mat4 objectMatrix = transform.worldMatrix;

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
    case GizmoOperation::TRANSLATE: operation = ImGuizmo::OPERATION::TRANSLATE;
        break;
    case GizmoOperation::ROTATE: operation = ImGuizmo::OPERATION::ROTATE;
        break;
    case GizmoOperation::SCALE: operation = ImGuizmo::OPERATION::SCALE;
        break;
    default: operation = ImGuizmo::OPERATION::TRANSLATE;
        break;
    }

    // Set snap values based on current operation
    std::array snapValues = {0.0f, 0.0f, 0.0f};
    if (m_UseSnap)
    {
        switch (m_GizmoOperation)
        {
            using enum GizmoOperation;
        case TRANSLATE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_TranslateSnap;
            break;
        case ROTATE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_RotateSnap;
            break;
        case SCALE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_ScaleSnap;
            break;
        }
    }

    // Scale mode must use LOCAL space (world scale doesn't make sense)
    ImGuizmo::MODE mode = (m_GizmoOperation == GizmoOperation::SCALE)
                              ? ImGuizmo::LOCAL
                              : static_cast<ImGuizmo::MODE>(std::to_underlying(m_GizmoSpace));

    // Render and manipulate the gizmo
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        operation,
        mode,
        glm::value_ptr(objectMatrix),
        nullptr,
        m_UseSnap ? snapValues.data() : nullptr
    );

    // If the gizmo was manipulated, decompose the result into local space.
    // objectMatrix now holds the NEW world matrix after Manipulate().
    // For child GOs we must factor out the parent's world transform so that
    // position/orientation/scale remain in the parent's local space.
    if (ImGuizmo::IsUsing())
    {
        auto parentInverse = glm::mat4(1.0f);
        if (GameObject parent = selected.GetParent(); parent.IsValid())
        {
            if (CTransform const* pt = parent.TryGetComponent<CTransform>())
                parentInverse = glm::inverse(pt->worldMatrix);
        }

        const glm::mat4 newLocalMatrix = parentInverse * objectMatrix;

        glm::vec3 newPosition;
        glm::vec3 newScale;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat newOrientation;
        glm::decompose(newLocalMatrix, newScale, newOrientation, newPosition, skew, perspective);

        transform.position = newPosition;
        transform.orientation = newOrientation;
        transform.scale = newScale;
        transform.eulerHint = transform.GetEulerAngles();
        transform.MarkDirty();

        transform.UpdateMatrix();
    }
}

void SceneViewport::HandleMousePicking(const ImVec2& viewportPos, const ImVec2& viewportSize) const
{
    // Only respond to left mouse button click when the viewport is hovered and
    // right mouse button is NOT held (camera uses RMB + WASD).
    if (!ImGui::IsWindowHovered() ||
        editorContext->GetInput()->GetMouseButton(SDL_BUTTON_LEFT) != KeyState::DOWN ||
        editorContext->GetInput()->GetMouseButton(SDL_BUTTON_RIGHT) != KeyState::IDLE)
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

    // Full UV — panel-relative coords map directly to framebuffer UV (no cropping).
    VulkanContext const* vkContext = VulkanBackend::GetVulkanContext();
    int32_t pixelX = std::clamp(static_cast<int32_t>(relX * static_cast<float>(vkContext->framebufferWidth) + 0.5f),
                                0, vkContext->framebufferWidth - 1);
    int32_t pixelY = std::clamp(static_cast<int32_t>(relY * static_cast<float>(vkContext->framebufferHeight) + 0.5f),
                                0, vkContext->framebufferHeight - 1);

    // Build geometry list (same logic as ModuleRenderer3D::BuildRenderPacket)
    if (!editorContext->GetScene()->activeScene)
        return;

    const auto gameObjects = editorContext->GetScene()->activeScene->GetGameObjectsSnapshot();
    std::vector<GeometryRenderData> geometries;
    geometries.reserve(gameObjects.size());

    for (auto go : gameObjects)
    {
        if (!go.HasComponent<CMesh>()) continue;

        GeometryRenderData data{};
        data.objectUID = go.GetID();

        if (auto const* transform = go.TryGetComponent<CTransform>())
            data.model = transform->worldMatrix;

        if (auto* mesh = go.TryGetComponent<CMesh>())
            data.geometry = mesh->mesh;

        geometries.emplace_back(data);
    }

    if (geometries.empty())
    {
        editorContext->GetScene()->selectedGameObject = {};
        return;
    }

    // Use the camera's own projection matrix — must match what the scene pass renders
    // with, so the pick result aligns with the scene framebuffer.
    // The UV mapping above already accounts for the viewport panel's crop.
    Camera const* cam = editorContext->GetCamera()->GetCamera();
    if (!cam) return;

    glm::mat4 projection = cam->GetProjectionMatrix();
    glm::mat4 view = cam->GetViewMatrix();

    // Perform the pick
    RendererFrontend const* frontend = editorContext->GetRendererFrontend();
    uint32_t objectID = frontend->PickObjectAt(pixelX, pixelY, projection, view, geometries);

    // Select or deselect
    if (objectID != 0)
    {
        GameObject found = editorContext->GetScene()->activeScene->FindGameObjectByID(objectID);
        if (found.IsValid())
        {
            editorContext->GetScene()->selectedGameObject = found;
        }
        else
        {
            // objectID didn't match any live GO — stale pick result; deselect.
            editorContext->GetScene()->selectedGameObject = {};
        }
    }
    else
    {
        editorContext->GetScene()->selectedGameObject = {};
    }
}

// Utility function to check if a string contains only valid ASCII characters.
bool SceneViewport::IsValidASCII(const std::string& str)
{
    return std::ranges::none_of(str, [](const unsigned char c)
    {
        return c > 127;
    });
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

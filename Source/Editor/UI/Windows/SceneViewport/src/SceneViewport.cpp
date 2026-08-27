#include "Editor/UI/Windows/SceneViewport/include/SceneViewport.h"

#include <algorithm>   // std::clamp

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Core/TypeRegistry.h>
#include <FileSystem/FileSystem.h>
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include <CameraSystem/Camera.h>

#include "Engine/Renderer/iEditorRenderBridge.h"

#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CMesh/CMesh.h>
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include <ECS/GameObject.h>
#include <ECS/Scene/Scene.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>
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

#include <Logger/Logger.h>
#include "SDL3/SDL.h"

SceneViewport::SceneViewport(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void SceneViewport::Init()
{
    CreateSceneViewportDescriptorSets(editorContext->GetEditorRenderBridge());
}

void SceneViewport::OnLayoutUpdated(const ImVec2& panelSize)
{
    // Drive camera aspect ratio from panel size — Hor+ scaling (vertical FOV fixed).
    // This keeps scene rendering, ImGuizmo, and mouse picking all consistent.
    if (Camera* editorCam = editorContext->GetCamera()->GetCamera())
        editorCam->SetAspectRatio(panelSize.x / panelSize.y);
}

ImGuiWindowFlags SceneViewport::GetWindowFlags() const
{
    // Prevent a floating window from being moved by ImGui while the gizmo is active.
    // Window movement is processed inside Begin(), so we must pass NoMove *to* Begin()
    // using the gizmo state from the previous frame (static bool persists across calls).
    return m_GizmoWasActive ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None;
}

void SceneViewport::DrawContent()
{
    editorContext->GetCamera()->sceneViewportHovered = ImGui::IsWindowHovered();

    // Handle gizmo mode switching (W/E/R keys) when viewport is hovered
    if (ImGui::IsWindowHovered() || ImGui::IsWindowFocused())
    {
        HandleGizmoInput();
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw gray background
    drawList->AddRectFilled(contentPos, contentEnd, IM_COL32(100, 100, 100, 255));

    if (contentSize.x <= 0.0f || contentSize.y <= 0.0f)
    {
        m_GizmoWasActive = false;
        return;
    }

    const IEditorRenderBridge* bridge = editorContext->GetEditorRenderBridge();
    if (!bridge)
    {
        m_GizmoWasActive = false;
        return;
    }

    constexpr ImVec2 uvMin(0.0f, 0.0f);
    constexpr ImVec2 uvMax(1.0f, 1.0f);

    // Position the image at the start of the content region and render
    ImGui::SetCursorPos(contentMin);
    ImGui::Image(bridge->GetViewportTexture(EditorViewport::Scene),
        contentSize, uvMin, uvMax);

    // Draw white border on top
    drawList->AddRect(contentPos, contentEnd, IM_COL32(255, 255, 255, 255));

    // Draw the gizmo on top of the scene image
    DrawGizmo(contentPos, contentSize);

    // ImGuizmo::IsOver() retains stale state from the previous frame when no
    // gizmo was drawn (e.g. nothing selected). Only consult it when a gizmo is
    // actually visible this frame, otherwise picking would be incorrectly blocked.
    GameObject primary = editorContext->GetScene()->primarySelection;
    const bool gizmoVisible  = primary.IsValid() && primary.HasComponent<CTransform>();
    const bool gizmoBlocking = gizmoVisible && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());

    // Keep the camera orbit target at the gizmo position (centroid of all selected objects).
    ModuleCamera3D* cam3D = editorContext->GetCamera();
    if (gizmoVisible)
    {
        const auto& selectedGOs = editorContext->GetScene()->selectedGameObjects;
        glm::vec3 centroid{0.0f};
        int count = 0;
        for (auto go : selectedGOs)
        {
            if (const auto* t = go.TryGetComponent<CTransform>())
            {
                centroid += glm::vec3(t->worldMatrix[3]);
                count++;
            }
        }
        if (count > 0)
            centroid /= static_cast<float>(count);
        cam3D->SetOrbitTarget(centroid);
    }
    else
    {
        cam3D->ClearOrbitTarget();
    }
    m_GizmoWasActive = gizmoBlocking;

    if (!gizmoBlocking)
    {
        // Handle mouse picking (click to select/deselect objects)
        HandleMousePicking(contentPos, contentSize);

        // Drag-and-drop target — only place the InvisibleButton when the gizmo
        // is not being hovered/used, so it doesn't steal mouse input from ImGuizmo
        HandleDragAndDropTarget();
    }
}

void SceneViewport::HandleDragAndDropTarget() const
{
    ImGui::SetCursorScreenPos(contentPos);
    ImGui::InvisibleButton("DropTarget", contentSize);

    if (!ImGui::BeginDragDropTarget())
    {
        return;
    }

    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

    if (payload == nullptr)
    {
        ImGui::EndDragDropTarget();
        return;
    }

    const auto* payload_data = static_cast<const char*>(payload->Data);
    const char* payload_end  = payload_data + payload->DataSize;
    std::vector<std::string> filePaths;

    // Payloads are null-terminated path runs; bound the walk by DataSize so we
    // never read past the buffer when the last path's terminator lands exactly
    // at the end (otherwise *payload_data reads uninitialized memory).
    while (payload_data < payload_end && *payload_data)
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
        const std::string ext = nous::engine::filesystem::GetExtension(path);
        if (editorContext->GetResourceManager()->GetTypeRegistry().TypeFromExtension(ext) == ResourceType::MESH)
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

    // Frame selected objects (F) — move camera to look at the centroid of all selected.
    // Distance is driven by the bounding radius: how far the objects are spread from the
    // centroid plus each object's own scale extent, so distant objects always fit in view.
    if (editorContext->GetInput()->GetKey(SDL_SCANCODE_F) == DOWN)
    {
        const auto& selectedGOs = editorContext->GetScene()->selectedGameObjects;
        glm::vec3 centroid{0.0f};
        float maxScale = 0.0f;
        int frameCount = 0;

        for (auto go : selectedGOs)
        {
            if (!go.HasComponent<CTransform>()) continue;
            const CTransform& t = go.GetComponent<CTransform>();
            centroid += glm::vec3(t.worldMatrix[3]);
            maxScale  = glm::max(maxScale, glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z)));
            frameCount++;
        }

        if (frameCount > 0)
        {
            centroid /= static_cast<float>(frameCount);

            // Second pass: measure how spread out the selection is from its centroid.
            float spreadRadius = 0.0f;
            for (auto go : selectedGOs)
            {
                if (!go.HasComponent<CTransform>()) continue;
                const CTransform& t = go.GetComponent<CTransform>();
                spreadRadius = glm::max(spreadRadius,
                    glm::length(glm::vec3(t.worldMatrix[3]) - centroid));
            }

            // Take the larger of two independent drivers:
            //  - object-size: maxScale * 500  (existing single-object formula, unchanged)
            //  - spread:      spreadRadius * 2.5  (~1/tan(30°) with padding for ~60° FOV)
            // For a single object spreadRadius == 0, so it reduces exactly to the original.
            const float distance = glm::max(glm::max(maxScale * 500.0f, spreadRadius * 2.5f), 500.0f);
            editorContext->GetCamera()->FrameTarget(centroid, distance);
        }
    }

    // Delete selected objects (Supr key)
    if ((editorContext->GetInput()->GetKey(SDL_SCANCODE_KP_PERIOD) == DOWN ||
        editorContext->GetInput()->GetKey(SDL_SCANCODE_PAUSE) == DOWN)
        && !ImGuizmo::IsUsing())
    {
        ModuleScene* scene = editorContext->GetScene();
        const std::vector<GameObject> toDelete = scene->selectedGameObjects;
        scene->ClearSelection();
        for (auto go : toDelete)
            scene->activeScene->DestroyGameObject(go);
    }

    // Toggle snapping with Left Ctrl
    m_UseSnap = (editorContext->GetInput()->GetKey(SDL_SCANCODE_LCTRL) == REPEAT
        || editorContext->GetInput()->GetKey(SDL_SCANCODE_LCTRL) == DOWN);
}

void SceneViewport::DrawGizmo(const ImVec2& viewportPos, const ImVec2& viewportSize) const
{
    ModuleScene* scene    = editorContext->GetScene();
    GameObject   selected = scene->primarySelection;
    if (!selected.IsValid() || !selected.HasComponent<CTransform>())
        return;

    Camera const* cam = editorContext->GetCamera()->GetCamera();
    if (!cam)
        return;

    glm::mat4 view       = cam->GetViewMatrix();
    glm::mat4 projection = cam->GetProjectionMatrix();

    CTransform& transform = selected.GetComponent<CTransform>();

    // Compute centroid of all selected objects that have a CTransform (world space).
    const auto& selectedGOs = scene->selectedGameObjects;
    glm::vec3 centroid{0.0f};
    int count = 0;
    for (auto go : selectedGOs)
    {
        if (const auto* t = go.TryGetComponent<CTransform>())
        {
            centroid += glm::vec3(t->worldMatrix[3]);
            count++;
        }
    }
    if (count > 1)
        centroid /= static_cast<float>(count);

    // Single-object: gizmo at the object's own world matrix.
    // Multi-object:  use a cached matrix that is fed back each frame so Manipulate
    //               receives the previous frame's output — making the decomposed delta
    //               incremental rather than total-from-drag-start.
    glm::mat4 objectMatrix;
    if (count > 1)
    {
        // Reset the cache when not dragging, or when the selection set size changed
        // mid-drag (e.g. Ctrl+click while holding LMB) to avoid applying a stale
        // centroid delta to a different set of objects.
        if (!ImGuizmo::IsUsing() || count != m_multiObjectGizmoCount)
            m_multiObjectGizmoMatrix = glm::translate(glm::mat4(1.0f), centroid);
        m_multiObjectGizmoCount = count;
        objectMatrix = m_multiObjectGizmoMatrix;
    }
    else
    {
        m_multiObjectGizmoCount = 0;
        objectMatrix = transform.worldMatrix;
    }

    // SetImGuiContext is critical in DLL architectures where ImGui and ImGuizmo
    // may not share the same global context.
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::Enable(true);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

    ImGuizmo::OPERATION operation;
    switch (m_GizmoOperation)
    {
    case GizmoOperation::TRANSLATE: operation = ImGuizmo::OPERATION::TRANSLATE; break;
    case GizmoOperation::ROTATE:    operation = ImGuizmo::OPERATION::ROTATE;    break;
    case GizmoOperation::SCALE:     operation = ImGuizmo::OPERATION::SCALE;     break;
    default:                        operation = ImGuizmo::OPERATION::TRANSLATE; break;
    }

    std::array snapValues = {0.0f, 0.0f, 0.0f};
    if (m_UseSnap)
    {
        switch (m_GizmoOperation)
        {
            using enum GizmoOperation;
        case TRANSLATE: snapValues[0] = snapValues[1] = snapValues[2] = m_TranslateSnap; break;
        case ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = m_RotateSnap;    break;
        case SCALE:     snapValues[0] = snapValues[1] = snapValues[2] = m_ScaleSnap;     break;
        }
    }

    ImGuizmo::MODE mode = (m_GizmoOperation == GizmoOperation::SCALE)
        ? ImGuizmo::LOCAL
        : static_cast<ImGuizmo::MODE>(std::to_underlying(m_GizmoSpace));

    // Capture the matrix before Manipulate modifies it so we can compute
    // the per-frame incremental delta in the multi-object path.
    const glm::mat4 prevMatrix = objectMatrix;

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        operation,
        mode,
        glm::value_ptr(objectMatrix),
        nullptr,
        m_UseSnap ? snapValues.data() : nullptr
    );

    if (!ImGuizmo::IsUsing())
        return;

    if (count == 1)
    {
        // Single-object path — factor out the parent's world transform to get local matrix.
        auto parentInverse = glm::mat4(1.0f);
        if (GameObject parent = selected.GetParent(); parent.IsValid())
        {
            if (const CTransform* pt = parent.TryGetComponent<CTransform>())
                parentInverse = glm::inverse(pt->worldMatrix);
        }

        const glm::mat4 newLocalMatrix = parentInverse * objectMatrix;

        glm::vec3 newPosition;
        glm::vec3 newScale;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat newOrientation;
        glm::decompose(newLocalMatrix, newScale, newOrientation, newPosition, skew, perspective);

        transform.position    = newPosition;
        transform.orientation = newOrientation;
        transform.scale       = newScale;
        transform.eulerHint   = transform.GetEulerAngles();
        transform.MarkDirty();
        transform.UpdateMatrix();
    }
    else
    {
        // Multi-object path — update cache then decompose the incremental delta
        // (prev→new) so we never re-apply the total drag offset.
        m_multiObjectGizmoMatrix = objectMatrix;

        glm::vec3 prevPos, prevScale, newPos, newScale, skew;
        glm::vec4 perspective;
        glm::quat prevRot, newRot;
        glm::decompose(prevMatrix,    prevScale, prevRot, prevPos, skew, perspective);
        glm::decompose(objectMatrix,  newScale,  newRot,  newPos,  skew, perspective);

        const glm::vec3 translationDelta = newPos - prevPos;
        const glm::quat rotationDelta    = newRot * glm::inverse(prevRot);
        constexpr float c_scaleEpsilon  = 1e-6f;
        const glm::vec3 scaleDelta = glm::vec3(
            glm::abs(prevScale.x) > c_scaleEpsilon ? newScale.x / prevScale.x : 1.0f,
            glm::abs(prevScale.y) > c_scaleEpsilon ? newScale.y / prevScale.y : 1.0f,
            glm::abs(prevScale.z) > c_scaleEpsilon ? newScale.z / prevScale.z : 1.0f
        );

        for (auto go : selectedGOs)
        {
            if (CTransform* t = go.TryGetComponent<CTransform>())
            {
                t->position    += translationDelta;
                t->orientation  = glm::normalize(rotationDelta * t->orientation);
                t->scale       *= scaleDelta;
                t->eulerHint    = t->GetEulerAngles();
                t->MarkDirty();
                t->UpdateMatrix();
            }
        }
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
    const IEditorRenderBridge* bridge = editorContext->GetEditorRenderBridge();
    if (!bridge)
        return;

    int32_t framebufferWidth = 0;
    int32_t framebufferHeight = 0;
    bridge->GetFramebufferSize(&framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0)
        return;

    int32_t pixelX = std::clamp(static_cast<int32_t>(relX * static_cast<float>(framebufferWidth) + 0.5f),
                                0, framebufferWidth - 1);
    int32_t pixelY = std::clamp(static_cast<int32_t>(relY * static_cast<float>(framebufferHeight) + 0.5f),
                                0, framebufferHeight - 1);

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
        editorContext->GetScene()->ClearSelection();
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

    // Select or deselect; Ctrl held = toggle, otherwise replace selection
    ModuleScene* scene = editorContext->GetScene();
    if (objectID != 0)
    {
        GameObject found = scene->activeScene->FindGameObjectByID(objectID);
        if (found.IsValid())
        {
            if (ImGui::GetIO().KeyCtrl)
            {
                scene->IsSelected(found) ? scene->RemoveFromSelection(found)
                                         : scene->AddToSelection(found);
            }
            else
            {
                scene->SetSelection(found);
            }
        }
        else
        {
            scene->ClearSelection();  // stale pick result
        }
    }
    else
    {
        scene->ClearSelection();  // click-nothing always clears
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

void SceneViewport::CreateSceneViewportDescriptorSets(IEditorRenderBridge* bridge)
{
    if (!bridge)
        return;

    // One descriptor set per swapchain image (runtime count) — size to match the images.
    const EditorViewportImages images = bridge->GetViewportImages(EditorViewport::Scene);

    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.reserve(images.views.size());

    for (const VkImageView view : images.views)
    {
        descriptorSets.push_back(ImGui_ImplVulkan_AddTexture(
            images.sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    bridge->SetViewportDescriptorSets(EditorViewport::Scene, std::move(descriptorSets));
}

void SceneViewport::DestroySceneViewportDescriptorSets(IEditorRenderBridge* bridge)
{
    if (!bridge)
        return;

    // Take-and-clear hands back exactly the sets that exist: the set count can be
    // smaller than the image count before Create has run, and removing textures by
    // the image count would read past the end. Destroy is idempotent — a second
    // call gets an empty vector.
    const std::vector<VkDescriptorSet> descriptorSets =
        bridge->TakeViewportDescriptorSets(EditorViewport::Scene);

    for (const VkDescriptorSet set : descriptorSets)
    {
        ImGui_ImplVulkan_RemoveTexture(set);
    }
}

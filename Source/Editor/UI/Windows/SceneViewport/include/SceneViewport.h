#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Editor/EditorExport.h"
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

struct ImVec2;
struct VulkanContext;

enum class GizmoOperation : uint8_t
{
    TRANSLATE,
    ROTATE,
    SCALE
};

// Maps to ImGuizmo::MODE values
enum class GizmoSpace : uint8_t
{
    LOCAL = 0,
    WORLD = 1
};

class SceneViewport : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit SceneViewport(const char* title, EditorContext* context, bool start_open = true);

    NOUS_EDITOR_API void Init() override;
    void OnLayoutUpdated(const ImVec2& panelSize) override;
    ImGuiWindowFlags GetWindowFlags() const override;
    NOUS_EDITOR_API void DrawContent() override;

    static void CreateSceneViewportDescriptorSets();
    static void DestroySceneViewportDescriptorSets();

private:

    static bool IsValidASCII(const std::string& str);
    void HandleDragAndDropTarget() const;

    void HandleGizmoInput();
    void HandleMousePicking(const ImVec2& viewportPos, const ImVec2& viewportSize) const;
    void DrawGizmo(const ImVec2& viewportPos, const ImVec2& viewportSize) const;

    GizmoOperation m_GizmoOperation = GizmoOperation::TRANSLATE;
    GizmoSpace m_GizmoSpace = GizmoSpace::WORLD;
    bool m_UseSnap = false;
    float m_TranslateSnap = 1.0f;
    float m_RotateSnap = 15.0f;
    float m_ScaleSnap = 0.1f;

    bool m_GizmoWasActive = false;

    // Cached gizmo state for multi-object manipulation.
    // Matrix is fed back as input to Manipulate each frame so the decomposed
    // delta is incremental (prev→new), not total-from-drag-start.
    // Count tracks the previous selection size to detect mid-drag changes.
    mutable glm::mat4 m_multiObjectGizmoMatrix = glm::mat4(1.0f);
    mutable int       m_multiObjectGizmoCount  = 0;

};
#ifndef SCENEVIEWPORT_H
#define SCENEVIEWPORT_H

#include "Editor/UI/IEditorWindow.inl"
#include "Editor/EditorExport.h"
#include <string>

struct ImVec2;
struct VulkanContext;

enum class GizmoOperation
{
    TRANSLATE,
    ROTATE,
    SCALE
};

// Maps to ImGuizmo::MODE values
enum class GizmoSpace
{
    LOCAL = 0,
    WORLD = 1
};

class SceneViewport : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit SceneViewport(const char* title, ::EditorContext* context, bool start_open = true);

    NOUS_EDITOR_API void Init() override;
    NOUS_EDITOR_API void Draw() override;

    static void CreateSceneViewportDescriptorSets();
    static void DestroySceneViewportDescriptorSets();

private:

    static bool IsValidASCII(const std::string& str);

    void HandleGizmoInput();
    void HandleMousePicking(const ImVec2& viewportPos, const ImVec2& viewportSize,
                            const ImVec2& uvMin, const ImVec2& uvMax);
    void DrawGizmo(const ImVec2& viewportPos, const ImVec2& viewportSize);

    GizmoOperation m_GizmoOperation = GizmoOperation::TRANSLATE;
    GizmoSpace m_GizmoSpace = GizmoSpace::WORLD;
    bool m_UseSnap = false;
    float m_TranslateSnap = 1.0f;
    float m_RotateSnap = 15.0f;
    float m_ScaleSnap = 0.1f;

};

#endif // SCENEVIEWPORT_H
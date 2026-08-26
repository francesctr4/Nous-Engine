#ifndef GAMEVIEWPORT_H
#define GAMEVIEWPORT_H

#include "Editor/UI/IEditorWindow.h"
#include "Editor/EditorExport.h"

class IEditorRenderBridge;

class GameViewport : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit GameViewport(const char* title, EditorContext* context, bool start_open = true);

    NOUS_EDITOR_API void Init() override;
    NOUS_EDITOR_API bool UpdatesWhenCollapsed() const override;
    NOUS_EDITOR_API void OnLayoutUpdated(const ImVec2& panelSize) override;
    NOUS_EDITOR_API void DrawContent() override;

protected:

    // Gates script input on this window's focus state. Called from inside ImGui's
    // window scope (after ImGui::Begin) because IsWindowFocused() only works there.
    NOUS_EDITOR_API bool Begin(bool& outVisible) override;

public:

    // Take the bridge as a parameter rather than reading it from an editor-side
    // static: ModuleEditor calls these from its IMGUI_RECREATION handler, where
    // it already holds the bridge.
    static void CreateGameViewportDescriptorSets(IEditorRenderBridge* bridge);
    static void DestroyGameViewportDescriptorSets(IEditorRenderBridge* bridge);
};

#endif // GAMEVIEWPORT_H
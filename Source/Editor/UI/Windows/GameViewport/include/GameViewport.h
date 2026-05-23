#ifndef GAMEVIEWPORT_H
#define GAMEVIEWPORT_H

#include "Editor/UI/IEditorWindow.h"
#include "Editor/EditorExport.h"

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

    static void CreateGameViewportDescriptorSets();
    static void DestroyGameViewportDescriptorSets();
};

#endif // GAMEVIEWPORT_H
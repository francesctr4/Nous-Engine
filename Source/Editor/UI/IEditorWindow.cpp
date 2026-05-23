#include "Editor/UI/IEditorWindow.h"

void IEditorWindow::Draw()
{
    if (!initialized) { Init(); initialized = true; }
    if (!*p_open) return;

    // 1. Update() runs every frame the window is "Open" (checked in a menu).
    // This is already independent of visibility.
    Update();

    bool visible = false;
    if (Begin(visible))
    {
        // 2. Decide if we should process layout/rendering logic.
        // We do this if the window is visible OR if it's a "Background Worker" like a Viewport.
        if (visible || UpdatesWhenCollapsed())
        {
            bool sizeChanged = UpdateLayout();

            if (layoutValid && sizeChanged)
                OnLayoutUpdated(contentSize);

            // 3. DrawContent() contains ImGui widgets (Buttons, Images).
            // This SHOULD stay visible-only because ImGui cannot draw widgets
            // into a collapsed window reliably.
            if (visible && layoutValid)
                DrawContent();
        }

        End();
    }

    FinishUpdate();
}

bool IEditorWindow::UpdateLayout()
{
    contentMin = ImGui::GetWindowContentRegionMin();
    const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

    ImVec2 newSize{
        contentMax.x - contentMin.x,
        contentMax.y - contentMin.y
    };

    const bool newValid =
        newSize.x > 0.0f &&
        newSize.y > 0.0f;

    if (newValid)
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        contentPos = {
            windowPos.x + contentMin.x,
            windowPos.y + contentMin.y
        };
        contentEnd = {
            contentPos.x + newSize.x,
            contentPos.y + newSize.y
        };
    }

    bool changed =
        newValid &&
        (newSize.x != contentSize.x ||
         newSize.y != contentSize.y);

    contentSize  = newSize;
    layoutValid  = newValid;

    return changed;
}
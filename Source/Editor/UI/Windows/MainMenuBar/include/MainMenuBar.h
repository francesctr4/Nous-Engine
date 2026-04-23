#ifndef MAINMENUBAR_H
#define MAINMENUBAR_H

#include "Editor/UI/IEditorWindow.h"

class MainMenuBar : public IEditorWindow
{
public:

    explicit MainMenuBar(const char* title, ::EditorContext* context, bool start_open = true);

    // Container Overrides
    bool Begin(bool& outVisible) override {
        outVisible = ImGui::BeginMainMenuBar();
        return outVisible; // MenuBar only calls End if Begin is true
    }
    void End() override { ImGui::EndMainMenuBar(); }

    bool UpdatesWhenCollapsed() const override;
    void Update() override;
    void DrawContent() override;
    void FinishUpdate() override;
    bool UpdateLayout() override
    {
        // Menu bar doesn't have a meaningful content region
        layoutValid = true;
        return false;
    }

private:

    void saveToCurrent(bool& triggerSaveAs) const;

    // Deferred popup opens — must happen outside BeginMenu, after EndMainMenuBar.
    bool openSaveAs   = false;
    bool openOpen     = false;
    bool openNewScene = false;

    static constexpr const char* kScenesDir     = "Assets/Scenes";
    static constexpr const char* kSceneExt      = ".nous";
    static constexpr const char* kSaveAsPopup   = "Save Scene As";
    static constexpr const char* kOpenPopup     = "Open Scene";
    static constexpr const char* kNewScenePopup = "New Scene";
};

#endif // MAINMENUBAR_H
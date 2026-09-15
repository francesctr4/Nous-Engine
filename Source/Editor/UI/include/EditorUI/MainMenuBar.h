#ifndef MAINMENUBAR_H
#define MAINMENUBAR_H

#include <EditorUI/IEditorWindow.h>

#include <string>
#include <vector>

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

    // The menu bar is a registered window like any other, so it would otherwise list
    // ITSELF under Windows -- offering the one close that cannot be undone, because
    // this menu is the only way to reopen anything.
    bool ShowInWindowsMenu() const override { return false; }

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
    void DrawBuildModal();
    void DrawBuildSettingsPopup();

    // ── Scene popup triggers ──────────────────────────────────────────────
    bool openSaveAs    = false;
    bool openOpen      = false;
    bool openNewScene  = false;
    bool openClearScene = false;

    // ── Build trigger flags (set in DrawContent, consumed in FinishUpdate) ──
    bool        m_openBuild         = false;
    bool        m_openBuildAndRun   = false;
    bool        m_openBuildSettings = false;

    // ── Build modal state ─────────────────────────────────────────────────
    bool        m_buildModalOpen    = false;
    bool        m_buildDone         = false;
    bool        m_buildSuccess      = false;
    bool        m_buildLaunchAfter  = false;
    std::string m_buildOutputPath;
    std::string m_buildStartupScene;
    std::vector<std::string> m_buildLog;

    // ── Editor layout (window positions, docking, sizes) ──────────────────
    //
    // The file ModuleEditor::Awake copies over imgui.ini at startup, so saving here
    // is what makes the current arrangement the one the editor OPENS with. Same
    // relative path as that copy, and so the same working-directory assumption.
    static constexpr const char* kEditorLayoutFile   = "Assets/Settings/imgui.ini";

    static constexpr const char* kScenesDir          = "Assets/Scenes";
    static constexpr const char* kSceneExt           = ".nous";
    static constexpr const char* kSaveAsPopup        = "Save Scene As";
    static constexpr const char* kOpenPopup          = "Open Scene";
    static constexpr const char* kNewScenePopup      = "New Scene";
    static constexpr const char* kClearScenePopup    = "Clear Scene";
    static constexpr const char* kBuildModal         = "###BuildGameModal";
    static constexpr const char* kBuildSettingsPopup = "Build Settings";
};

#endif // MAINMENUBAR_H

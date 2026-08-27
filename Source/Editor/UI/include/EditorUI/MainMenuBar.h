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
    bool openSaveAs   = false;
    bool openOpen     = false;
    bool openNewScene = false;

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

    static constexpr const char* kScenesDir          = "Assets/Scenes";
    static constexpr const char* kSceneExt           = ".nous";
    static constexpr const char* kSaveAsPopup        = "Save Scene As";
    static constexpr const char* kOpenPopup          = "Open Scene";
    static constexpr const char* kNewScenePopup      = "New Scene";
    static constexpr const char* kBuildModal         = "###BuildGameModal";
    static constexpr const char* kBuildSettingsPopup = "Build Settings";
};

#endif // MAINMENUBAR_H

#pragma once

#include <EditorUI/IEditorWindow.h>

// Step-6 mixer: a left routing panel + a row of vertical channel strips (Master ·
// Music · SFX · UI · Ambient), each a dB fader + Solo/Mute toggles, driving
// ModuleAudio's bus ops live. The routing panel lists the scene's CAudioSources
// grouped by their targetBus, with a live ">" mark while a source is actually
// playing (a pure ECS read — no new runtime). Reaches ModuleAudio/Scene via
// GetContext()->GetScene(). Holds no persistent state — the backend's bus graph is
// the source of truth. Editor-mode only.
//
// Deferred (post-MVP, asset-driven mixer): multiple mixer assets, an editable groups
// tree, snapshots, saved views, per-bus effect slots, Bypass, exposed parameters. dB
// is a UI-only concern — the backend stores linear gain.
class AudioMixerWindow : public IEditorWindow
{
public:

    explicit AudioMixerWindow(const char* title, EditorContext* context, bool start_open = true);

    void Update() override;
    void DrawContent() override;
};

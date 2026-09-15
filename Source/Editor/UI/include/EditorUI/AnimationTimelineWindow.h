#pragma once

#include <EditorUI/IEditorWindow.h>
#include <EditorUI/AnimationTimelineLayout.h>
#include <EditorCore/EditorExport.h>

#include <cstdint>
#include <string>

class CAnimator;
class ModuleResourceManager;
class ResourceAnimation;

/**
 * @brief Places animation event markers on a clip's timeline, against the pose.
 *
 * Structurally AnimationControllerEditor: same base class, same selection-driven
 * target, same explicit Save. What differs is that the arithmetic lives in the
 * separately tested AnimationTimelineLayout.h rather than in this file -- so what the
 * tests drive is what the editor runs.
 *
 * SCRUBBING ARMS THE ANIMATOR rather than sampling here: the render packet is built
 * from CAnimator::GetPalette() in ModuleRenderer3D::PostUpdate, before editor windows
 * draw, so a pose written at draw time would show up one frame late.
 */
class AnimationTimelineWindow : public IEditorWindow
{
public:
    NOUS_EDITOR_API explicit AnimationTimelineWindow(const char* title, EditorContext* context,
                                                     bool start_open = true);

    // Releases the dropped clip's reference. The window is deleted by ModuleEditor at
    // shutdown, which is before the resource manager goes away.
    NOUS_EDITOR_API ~AnimationTimelineWindow() override;

protected:
    NOUS_EDITOR_API void DrawContent() override;

private:
    [[nodiscard]] ModuleResourceManager* ResourceManager() const;

    // Resolves the selected GameObject's animator, or null. Re-resolved every frame:
    // selection changes, a component is removed, a scene reloads -- and a remembered
    // pointer would outlive any of those.
    [[nodiscard]] CAnimator* ResolveAnimator() const;

    // The clip on the ruler: the dropped one if there is one, else the state chosen in
    // the combo. Re-resolved from the controller each frame for the same reason.
    [[nodiscard]] ResourceAnimation* ResolveClip(CAnimator& animator) const;

    // Acquires `nanimPath` into m_droppedClip and releases whatever it held. Empty
    // clears. ACQUIRE THEN RELEASE, unconditionally: re-resolving the same clip finds
    // it resident and only increments, so a change-detecting release never fires, and
    // releasing first would let the count transiently hit 0 and queue a spurious
    // eviction of the very clip being kept.
    void SetDroppedClip(const std::string& nanimPath);

    void DrawClipPicker(CAnimator& animator);
    void DrawRuler(CAnimator& animator, ResourceAnimation& clip);
    void DrawSelectedMarkerPanel(ResourceAnimation& clip);

    // Index into the controller's state list whose clip is shown.
    int m_stateIndex = 0;

    // A clip dragged in from the Assets Browser, for one no state plays yet -- which is
    // also the only way to reach this window's editing at all when the character has no
    // controller. OWNED: there is no non-acquiring path lookup on the resource manager,
    // so showing a clip means holding a reference to it.
    ResourceAnimation* m_droppedClip = nullptr;

    // Whether scrubbing drives the character's pose.
    //
    // OFF BY DEFAULT, unlike most toggles, because the window itself opens by default:
    // an on-by-default preview would hold every selected character at its playhead from
    // the moment the editor starts, which reads as the animation being broken rather
    // than as a preview being on. Opting in costs one click at the point where the user
    // already means to scrub.
    //
    // It OVERRIDES the graph, so while the scene is playing the character visibly stops
    // animating and holds the playhead's pose. That is the feature, not a freeze, and
    // this is the switch that turns it off; closing the window does the same, since the
    // arm expires on its own. Suppressing it by simulation state instead was tried and
    // removed the case the window is most used in.
    bool  m_preview       = false;

    float m_playhead      = 0.0f;
    int   m_selectedEvent = -1;
    int   m_draggedEvent  = -1;

    // Held across frames because a scrub is a DRAG, not a click: the ruler only ever
    // reacted to IsMouseClicked, which is why the playhead jumped to the cursor and
    // then stopped following it. Mirrors m_draggedEvent, and is released by the same
    // mouse-up, so the two can never both be live.
    bool  m_scrubbing     = false;
    float m_snapPerSecond = 30.0f;   // 0 = off
    bool  m_dirty         = false;   // unsaved marker edits
};

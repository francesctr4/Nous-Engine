#include <EditorUI/AnimationTimelineWindow.h>

#include <ModuleScene/ModuleScene.h>
#include <ModuleResourceManager/ModuleResourceManager.h>

#include <ECS/GameObject.h>
#include <ECS/Scene/Scene.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <EngineCore/Casts.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceAnimation/ImporterAnimation.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

using namespace nous::editor::timeline;
using nous::engine::animation_system::AnimationEvent;

namespace
{
    constexpr float c_rulerHeight   = 74.0f;
    constexpr float c_grabRadius    = 7.0f;
    constexpr float c_handleHeight  = 10.0f;   // the playhead's draggable caret
    constexpr float c_labelRow      = 15.0f;   // text row above the track
    constexpr float c_fieldWidth    = 220.0f;  // marker panel fields

    // A label needs room for its own text plus a gap; below this two labels touch and
    // the ruler reads as noise. Feeds ChooseTickStep, which is what keeps the grid
    // legible at any window width.
    constexpr float c_minLabelSpacing = 64.0f;

    const ImU32 c_colTrack      = IM_COL32( 30,  30,  34, 255);
    const ImU32 c_colTrackEdge  = IM_COL32( 64,  64,  72, 255);
    const ImU32 c_colTickMajor  = IM_COL32(112, 112, 120, 255);
    const ImU32 c_colTickMinor  = IM_COL32( 70,  70,  78, 255);
    const ImU32 c_colLabel      = IM_COL32(150, 150, 158, 255);
    const ImU32 c_colMarker     = IM_COL32(120, 190, 255, 255);
    const ImU32 c_colMarkerSel  = IM_COL32(255, 196,  64, 255);
    const ImU32 c_colPlayhead   = IM_COL32(244, 100,  92, 255);

    // The Assets Browser payload is a NUL-separated LIST, not one string -- a
    // multi-select drop carries several paths. Reading it as a bare c-string would
    // silently take whichever happened to be first and ignore its extension.
    std::string FirstDroppedAsset(const ImGuiPayload* payload, const char* ext)
    {
        const char* data = static_cast<const char*>(payload->Data);
        const char* end  = data + payload->DataSize;

        while (data < end && *data)
        {
            std::string path(data);
            data += path.size() + 1;

            if (std::filesystem::path(path).extension().string() == ext)
                return path;
        }
        return {};
    }
}

AnimationTimelineWindow::AnimationTimelineWindow(const char* title, EditorContext* context,
                                                 const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

AnimationTimelineWindow::~AnimationTimelineWindow()
{
    SetDroppedClip({});
}

ModuleResourceManager* AnimationTimelineWindow::ResourceManager() const
{
    return editorContext ? editorContext->GetResourceManager() : nullptr;
}

CAnimator* AnimationTimelineWindow::ResolveAnimator() const
{
    if (!editorContext) return nullptr;

    const ModuleScene* mScene = editorContext->GetScene();
    if (!mScene) return nullptr;

    GameObject go = mScene->primarySelection;
    if (!go.IsValid()) return nullptr;

    return go.TryGetComponent<CAnimator>();
}

void AnimationTimelineWindow::SetDroppedClip(const std::string& nanimPath)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm) return;

    ResourceAnimation* acquired = nullptr;
    if (!nanimPath.empty())
    {
        // Null-tested BEFORE the cast: down_cast asserts on null, and a path that does
        // not resolve is an ordinary outcome here, not a type error.
        if (ResourceBase* r = rm->CreateResource(nanimPath))
            acquired = down_cast<ResourceAnimation*>(r);
    }

    ResourceAnimation* previous = m_droppedClip;
    m_droppedClip = acquired;

    if (previous)
        rm->UnloadResource(previous->GetUID());

    m_selectedEvent = -1;
    m_draggedEvent  = -1;
    m_playhead      = 0.0f;
}

ResourceAnimation* AnimationTimelineWindow::ResolveClip(CAnimator& animator) const
{
    if (m_droppedClip) return m_droppedClip;

    if (!animator.controller) return nullptr;

    const auto& states = animator.controller->graph.states;
    if (m_stateIndex < 0 || static_cast<size_t>(m_stateIndex) >= states.size()) return nullptr;

    const int clipIndex = states[m_stateIndex].clipIndex;
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= animator.controller->clips.size())
        return nullptr;

    return animator.controller->clips[clipIndex];
}

void AnimationTimelineWindow::DrawContent()
{
    CAnimator* animator = ResolveAnimator();
    if (!animator)
    {
        ImGui::TextDisabled("Select a GameObject with a CAnimator.");
        return;
    }

    DrawClipPicker(*animator);

    ResourceAnimation* clip = ResolveClip(*animator);
    if (!clip)
    {
        // Not a cosmetic gap: a state with no clip is the single most common reason a
        // character stands in bind pose with no other symptom.
        ImGui::TextDisabled("This state has no clip. Pick another, or drop a .nanim here.");
        return;
    }

    ImGui::Separator();
    DrawRuler(*animator, *clip);
    ImGui::Separator();
    DrawSelectedMarkerPanel(*clip);
}

void AnimationTimelineWindow::DrawClipPicker(CAnimator& animator)
{
    // The clip list comes off the CONTROLLER, not a slot of its own, so the window
    // shows exactly the clips this character can actually reach.
    if (animator.controller && !animator.controller->graph.states.empty())
    {
        const auto& states = animator.controller->graph.states;
        const int   count  = static_cast<int>(states.size());
        const int   shown  = std::clamp(m_stateIndex, 0, count - 1);

        ImGui::BeginDisabled(m_droppedClip != nullptr);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("State", states[shown].name.c_str()))
        {
            for (int i = 0; i < count; ++i)
                if (ImGui::Selectable(states[i].name.c_str(), i == shown))
                {
                    m_stateIndex    = i;
                    m_selectedEvent = -1;
                    m_playhead      = 0.0f;
                }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    }
    else
    {
        ImGui::TextDisabled("No controller. Drop a .nanim to edit its events.");
    }

    ImGui::SameLine();
    ImGui::Button(m_droppedClip ? "Drop .nanim (override)" : "Drop .nanim here");
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
            if (const std::string nanim = FirstDroppedAsset(payload, ".nanim"); !nanim.empty())
                SetDroppedClip(nanim);

        ImGui::EndDragDropTarget();
    }

    if (m_droppedClip)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Back to states")) SetDroppedClip({});
    }
}

void AnimationTimelineWindow::DrawRuler(CAnimator& animator, ResourceAnimation& clip)
{
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  width  = std::max(ImGui::GetContentRegionAvail().x - 16.0f, 32.0f);

    const RulerLayout ruler{ origin.x + 8.0f, width, clip.clip.duration };

    ImGui::InvisibleButton("##timeline", ImVec2(width + 16.0f, c_rulerHeight));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Three bands, top to bottom: the caret's handle, the labelled tick row, and the
    // track the markers live in. Separating the handle from the marker lane is what
    // lets the playhead be grabbed even when it sits on top of a marker.
    const float top      = origin.y + 3.0f;
    const float labelTop = top + c_handleHeight;
    const float trackTop = labelTop + c_labelRow;
    const float bottom   = origin.y + c_rulerHeight - 6.0f;

    draw->AddRectFilled(ImVec2(ruler.x, trackTop), ImVec2(ruler.x + ruler.width, bottom),
                        c_colTrack, 4.0f);
    draw->AddRect(ImVec2(ruler.x, trackTop), ImVec2(ruler.x + ruler.width, bottom),
                  c_colTrackEdge, 4.0f);

    // A LABELLED grid, derived from the ruler's own width rather than from the snap
    // setting. The old version drew one bare tick per snap step, so a 30/s grid over a
    // 2 s clip was 60 identical marks that said nothing about where you were.
    const float step = ChooseTickStep(clip.clip.duration, ruler.width, c_minLabelSpacing);
    if (step > 0.0f)
    {
        // Four subdivisions per labelled step, and only while they stay 4 px apart --
        // below that they fill in solid and hide the markers they sit behind.
        const float minorStep = step * 0.25f;
        const bool  drawMinor = (ruler.width * minorStep / clip.clip.duration) > 4.0f;

        for (float t = 0.0f; t <= clip.clip.duration; t += step)
        {
            const float x = TimeToX(ruler, t);
            draw->AddLine(ImVec2(x, trackTop), ImVec2(x, bottom), c_colTickMajor);

            // Whole seconds lose their decimals: "2 s" reads faster than "2.00 s", and
            // a sub-second step needs exactly as many places as the step itself has.
            char label[32];
            std::snprintf(label, sizeof(label), step >= 1.0f ? "%.0f s" : "%.2f s", t);
            draw->AddText(ImVec2(x + 3.0f, labelTop), c_colLabel, label);

            if (drawMinor)
                for (int m = 1; m < 4; ++m)
                {
                    const float mx = TimeToX(ruler, t + minorStep * static_cast<float>(m));
                    if (mx > ruler.x + ruler.width) break;
                    draw->AddLine(ImVec2(mx, bottom - 6.0f), ImVec2(mx, bottom),
                                  c_colTickMinor);
                }
        }
    }

    std::vector<float> times;
    times.reserve(clip.events.size());
    for (const AnimationEvent& event : clip.events) times.push_back(event.time);

    const float mouseX    = ImGui::GetIO().MousePos.x;
    const float playheadX = TimeToX(ruler, m_playhead);
    const int   hovering  = hovered ? HitTestMarker(ruler, times, mouseX, c_grabRadius) : -1;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // A MARKER WINS over the playhead when the two overlap. Markers are what this
        // window edits, and a scrub is recoverable in one click while a marker dragged
        // by accident has already moved.
        if (hovering >= 0)
        {
            m_selectedEvent = hovering;
            m_draggedEvent  = hovering;
        }
        else
        {
            // Clicking bare track begins a scrub as well as jumping to it, so grabbing
            // the caret and dragging the track are the same gesture rather than two --
            // there is nothing to discover, and no dead zone to miss.
            m_scrubbing     = true;
            m_selectedEvent = -1;
        }
    }

    if (m_draggedEvent >= 0 && static_cast<size_t>(m_draggedEvent) < clip.events.size()
        && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const float raw = XToTime(ruler, mouseX);
        clip.events[m_draggedEvent].time =
            ClampTime(SnapTime(raw, m_snapPerSecond), clip.clip.duration);
        m_playhead = clip.events[m_draggedEvent].time;
        m_dirty    = true;
    }

    // Read from the mouse EVERY frame of the drag, not once on the click. Snapped like
    // a marker is, because the playhead is where Add Event places one -- an unsnapped
    // scrub would author markers off the grid the snap setting exists to impose.
    if (m_scrubbing)
        m_playhead = ClampTime(SnapTime(XToTime(ruler, mouseX), m_snapPerSecond),
                               clip.clip.duration);

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_draggedEvent = -1;
        m_scrubbing    = false;
    }

    // Says "this is draggable" before it is tried, which is the whole reason the caret
    // has a handle wide enough to aim at.
    const bool overPlayhead = hovered && std::abs(mouseX - playheadX) <= c_grabRadius;
    if (m_scrubbing || m_draggedEvent >= 0 || overPlayhead || hovering >= 0)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    for (size_t i = 0; i < clip.events.size(); ++i)
    {
        const float x        = TimeToX(ruler, clip.events[i].time);
        const bool  selected = static_cast<int>(i) == m_selectedEvent;
        const ImU32 colour   = selected ? c_colMarkerSel : c_colMarker;

        draw->AddLine(ImVec2(x, trackTop + 2.0f), ImVec2(x, bottom), colour,
                      selected ? 2.0f : 1.0f);
        draw->AddTriangleFilled(ImVec2(x, trackTop + 12.0f),
                                ImVec2(x - 5.0f, trackTop + 2.0f),
                                ImVec2(x + 5.0f, trackTop + 2.0f), colour);
    }

    if (hovering >= 0 && m_draggedEvent < 0)
        ImGui::SetTooltip("%s  @ %.3f s", clip.events[hovering].name.c_str(),
                          clip.events[hovering].time);

    // Drawn LAST so it reads as being on top of the markers it crosses, and with its
    // handle above the marker lane so the two never compete for the same pixels.
    draw->AddLine(ImVec2(playheadX, labelTop), ImVec2(playheadX, bottom), c_colPlayhead, 2.0f);
    draw->AddTriangleFilled(ImVec2(playheadX - 6.0f, top),
                            ImVec2(playheadX + 6.0f, top),
                            ImVec2(playheadX, top + c_handleHeight), c_colPlayhead);

    if (m_scrubbing) ImGui::SetTooltip("%.3f s", m_playhead);

    // Re-armed EVERY FRAME, because an arm lasts exactly one OnUpdate -- that expiry is
    // what makes closing this window enough to release the character, since a closed
    // window is not called at all and has no hook that could disarm.
    //
    // Gated on the CHECKBOX ALONE, not on the simulation state: while stopped the preview
    // is the only way to see the frame a marker sits on, and while playing it is how a
    // marker is checked against the pose the graph is producing.
    if (m_preview)
        animator.SetPreview(&clip, m_playhead);

    // Scrubbing lives on the ruler, so what is left here is the typed-time escape hatch
    // and the three things that act on the playhead. A DragFloat rather than a slider:
    // ctrl-click is how an exact time gets typed, the one thing dragging a caret cannot do.
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::DragFloat("##time", &m_playhead, 0.005f, 0.0f, clip.clip.duration, "%.3f s"))
        m_playhead = ClampTime(m_playhead, clip.clip.duration);

    ImGui::SameLine();
    ImGui::TextDisabled("/ %.3f s", clip.clip.duration);

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::Checkbox("Preview", &m_preview);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Holds the character at the playhead's pose, in either\n"
                          "simulation state. Expires when this window closes.");

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::SetNextItemWidth(110.0f);
    const char* snapNames[]  = { "Snap: off", "24 / s", "30 / s", "60 / s" };
    const float snapValues[] = { 0.0f, 24.0f, 30.0f, 60.0f };
    int snapIndex = 0;
    for (int i = 0; i < 4; ++i) if (snapValues[i] == m_snapPerSecond) snapIndex = i;
    if (ImGui::Combo("##snap", &snapIndex, snapNames, 4))
        m_snapPerSecond = snapValues[snapIndex];

    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Button("Add Event"))
    {
        clip.events.push_back({ ClampTime(m_playhead, clip.clip.duration),
                                "NewEvent", 0.0f, "" });
        m_selectedEvent = static_cast<int>(clip.events.size()) - 1;
        m_dirty         = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        // Writes the stub AND the binary in one call -- they are only correct
        // together, and a disagreement stays until the next re-import picks a winner.
        if (ImporterAnimation::SaveAuthoring(clip)) m_dirty = false;
    }

    // Beside the button rather than inside its label: a label that changes width makes
    // everything after it move, and the state being reported is the CLIP's, not the
    // button's.
    if (m_dirty)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.77f, 0.25f, 1.0f), "unsaved");
    }
}

void AnimationTimelineWindow::DrawSelectedMarkerPanel(ResourceAnimation& clip)
{
    if (m_selectedEvent < 0 || static_cast<size_t>(m_selectedEvent) >= clip.events.size())
    {
        ImGui::TextDisabled("Click a marker to edit it, or drag it along the ruler.");
        return;
    }

    AnimationEvent& event = clip.events[m_selectedEvent];

    // The selected marker's own colour, so the panel and the flag on the ruler are
    // visibly the same object.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.77f, 0.25f, 1.0f));
    ImGui::SeparatorText(event.name.empty() ? "Marker" : event.name.c_str());
    ImGui::PopStyleColor();

    // EVERY field gets an explicit width. ImGui puts a label to the RIGHT of its
    // widget, so a full-width field pushes its own label to the far edge of the
    // window -- which is what made this panel read as a broken table.
    char nameBuffer[64] = {};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", event.name.c_str());
    ImGui::SetNextItemWidth(c_fieldWidth);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
    {
        event.name = nameBuffer;
        m_dirty    = true;
    }

    float time = event.time;
    ImGui::SetNextItemWidth(c_fieldWidth);
    if (ImGui::DragFloat("Time", &time, 0.005f, 0.0f, clip.clip.duration, "%.3f s"))
    {
        event.time = ClampTime(time, clip.clip.duration);
        m_dirty    = true;
    }

    // Seconds AND the frame it lands on, because markers are authored against frames --
    // a foot plants on a frame, not at 0.916 s. Only while snapping is on: with no grid
    // there is no frame rate to count in, and inventing one (30?) would be a number the
    // clip never agreed to.
    if (m_snapPerSecond > 0.0f)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("frame %d", static_cast<int>(event.time * m_snapPerSecond + 0.5f));
    }

    // NAMED AFTER THE SCRIPT'S PARAMETERS, not after their types. "Float" says what it
    // is; "floatParam" says where it arrives -- it is the identifier in
    // OnAnimationEvent(name, floatParam, stringParam), so the panel and the handler
    // spell the payload the same way.
    ImGui::SetNextItemWidth(c_fieldWidth);
    if (ImGui::DragFloat("floatParam", &event.floatParam, 0.01f)) m_dirty = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Passed to OnAnimationEvent untouched. The engine never\n"
                          "reads it -- what it means is the script's decision.");

    char stringBuffer[64] = {};
    std::snprintf(stringBuffer, sizeof(stringBuffer), "%s", event.stringParam.c_str());
    ImGui::SetNextItemWidth(c_fieldWidth);
    // A hint rather than a placeholder value: an empty payload is the common case, and
    // this is what tells it apart from a field that failed to load.
    if (ImGui::InputTextWithHint("stringParam", "optional -- L / R, surface, ...",
                                 stringBuffer, sizeof(stringBuffer)))
    {
        event.stringParam = stringBuffer;
        m_dirty           = true;
    }

    // THE CALL THIS MARKER MAKES, in the engine's own syntax. The panel cannot say what
    // a payload MEANS -- that is the script's business and deliberately unknown here --
    // but it can say exactly what crosses into the script DLL, which is the question a
    // handler is written against. It is also the fastest way to spot a payload left
    // over from another marker.
    ImGui::Spacing();
    char call[192];
    std::snprintf(call, sizeof(call), "OnAnimationEvent(\"%s\", %.3ff, \"%s\")",
                  event.name.c_str(), event.floatParam, event.stringParam.c_str());
    ImGui::TextDisabled("%s", call);

    ImGui::Spacing();
    if (ImGui::Button("Delete Event"))
    {
        clip.events.erase(clip.events.begin() + m_selectedEvent);
        m_selectedEvent = -1;
        m_draggedEvent  = -1;
        m_dirty         = true;
    }
}

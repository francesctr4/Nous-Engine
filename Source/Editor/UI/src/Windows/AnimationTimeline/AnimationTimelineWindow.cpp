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
#include <cstdio>
#include <filesystem>
#include <vector>

using namespace nous::editor::timeline;
using nous::engine::animation_system::AnimationEvent;

namespace
{
    constexpr float c_rulerHeight = 46.0f;
    constexpr float c_grabRadius  = 7.0f;

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
    const float top    = origin.y + 6.0f;
    const float bottom = origin.y + c_rulerHeight - 6.0f;

    draw->AddRectFilled(ImVec2(ruler.x, top), ImVec2(ruler.x + ruler.width, bottom),
                        IM_COL32(38, 38, 42, 255));

    // One tick per snap step, or per tenth of the clip when snapping is off. Skipped
    // entirely when the steps would be closer than 3 px: a 60/s grid on a long clip is
    // a solid bar that hides the markers it exists to help place.
    const float step = m_snapPerSecond > 0.0f ? 1.0f / m_snapPerSecond
                                              : clip.clip.duration * 0.1f;
    if (step > 0.0f && clip.clip.duration > 0.0f && (ruler.width * step / clip.clip.duration) > 3.0f)
        for (float t = 0.0f; t <= clip.clip.duration; t += step)
        {
            const float x = TimeToX(ruler, t);
            draw->AddLine(ImVec2(x, bottom - 5.0f), ImVec2(x, bottom),
                          IM_COL32(90, 90, 96, 255));
        }

    std::vector<float> times;
    times.reserve(clip.events.size());
    for (const AnimationEvent& event : clip.events) times.push_back(event.time);

    const float mouseX = ImGui::GetIO().MousePos.x;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const int hit = HitTestMarker(ruler, times, mouseX, c_grabRadius);
        if (hit >= 0)
        {
            m_selectedEvent = hit;
            m_draggedEvent  = hit;
        }
        else
        {
            m_playhead      = ClampTime(XToTime(ruler, mouseX), clip.clip.duration);
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

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_draggedEvent = -1;

    for (size_t i = 0; i < clip.events.size(); ++i)
    {
        const float x = TimeToX(ruler, clip.events[i].time);
        const ImU32 colour = (static_cast<int>(i) == m_selectedEvent)
                           ? IM_COL32(255, 196, 64, 255) : IM_COL32(120, 190, 255, 255);

        draw->AddTriangleFilled(ImVec2(x, top), ImVec2(x - 5.0f, top + 9.0f),
                                ImVec2(x + 5.0f, top + 9.0f), colour);
        draw->AddLine(ImVec2(x, top + 9.0f), ImVec2(x, bottom), colour);
    }

    const float playheadX = TimeToX(ruler, m_playhead);
    draw->AddLine(ImVec2(playheadX, top), ImVec2(playheadX, bottom),
                  IM_COL32(240, 240, 240, 255), 2.0f);

    // Re-armed EVERY FRAME, because an arm lasts exactly one OnUpdate. That expiry is
    // what makes closing this window enough to release the character -- a closed
    // IEditorWindow is not called at all, so there is no hook here that could disarm.
    //
    // Gated on the CHECKBOX ALONE, deliberately not on the simulation state. Holding
    // the pose is what a preview IS, in either state: while stopped it is the only way
    // to see the frame a marker sits on, and while playing it is how a marker is
    // checked against a pose the graph is actually producing. A stopped-scene-only rule
    // was tried and simply removed the case the window is most used in.
    if (m_preview)
        animator.SetPreview(&clip, m_playhead);

    ImGui::Checkbox("Preview", &m_preview);
    ImGui::SameLine();
    ImGui::TextDisabled("(holds the pose at the playhead)");

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Time", &m_playhead, 0.0f, clip.clip.duration, "%.3f s");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    const char* snapNames[]  = { "Snap: off", "24 / s", "30 / s", "60 / s" };
    const float snapValues[] = { 0.0f, 24.0f, 30.0f, 60.0f };
    int snapIndex = 0;
    for (int i = 0; i < 4; ++i) if (snapValues[i] == m_snapPerSecond) snapIndex = i;
    if (ImGui::Combo("##snap", &snapIndex, snapNames, 4))
        m_snapPerSecond = snapValues[snapIndex];

    ImGui::SameLine();
    if (ImGui::Button("Add Event"))
    {
        clip.events.push_back({ ClampTime(m_playhead, clip.clip.duration),
                                "NewEvent", 0.0f, "" });
        m_selectedEvent = static_cast<int>(clip.events.size()) - 1;
        m_dirty         = true;
    }

    ImGui::SameLine();
    if (ImGui::Button(m_dirty ? "Save *" : "Save"))
    {
        // Writes the stub AND the binary in one call -- they are only correct
        // together, and a disagreement stays until the next re-import picks a winner.
        if (ImporterAnimation::SaveAuthoring(clip)) m_dirty = false;
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

    char nameBuffer[64] = {};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", event.name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
    {
        event.name = nameBuffer;
        m_dirty    = true;
    }

    float time = event.time;
    if (ImGui::DragFloat("Time", &time, 0.005f, 0.0f, clip.clip.duration, "%.3f s"))
    {
        event.time = ClampTime(time, clip.clip.duration);
        m_dirty    = true;
    }

    if (ImGui::DragFloat("Float", &event.floatParam, 0.01f)) m_dirty = true;

    char stringBuffer[64] = {};
    std::snprintf(stringBuffer, sizeof(stringBuffer), "%s", event.stringParam.c_str());
    if (ImGui::InputText("String", stringBuffer, sizeof(stringBuffer)))
    {
        event.stringParam = stringBuffer;
        m_dirty           = true;
    }

    if (ImGui::Button("Delete Event"))
    {
        clip.events.erase(clip.events.begin() + m_selectedEvent);
        m_selectedEvent = -1;
        m_draggedEvent  = -1;
        m_dirty         = true;
    }
}

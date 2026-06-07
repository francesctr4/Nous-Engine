#include "Editor/UI/Windows/AudioGraphEditor/include/AudioGraphEditor.h"

#include <imgui.h>

#include <algorithm>
#include <span>

namespace ed = ax::NodeEditor;

// ---------------------------------------------------------------------------
// Category metadata
// ---------------------------------------------------------------------------

AudioNodeCategory AudioGraphEditor::GetCategory(AudioNodeKind kind)
{
    switch (kind)
    {
        case AudioNodeKind::AudioSource: return AudioNodeCategory::Source;
        case AudioNodeKind::AudioOutput: return AudioNodeCategory::Output;
        case AudioNodeKind::LowPass:
        case AudioNodeKind::HighPass:
        case AudioNodeKind::Delay:       return AudioNodeCategory::Effect;
        case AudioNodeKind::Gain:        return AudioNodeCategory::Utility;
    }
    return AudioNodeCategory::Utility;
}

const char* AudioGraphEditor::GetKindLabel(AudioNodeKind kind)
{
    switch (kind)
    {
        case AudioNodeKind::AudioSource: return "Audio Source";
        case AudioNodeKind::AudioOutput: return "Audio Output";
        case AudioNodeKind::LowPass:     return nous::audio::DisplayName(AudioEffectType::LowPass);
        case AudioNodeKind::HighPass:    return nous::audio::DisplayName(AudioEffectType::HighPass);
        case AudioNodeKind::Delay:       return nous::audio::DisplayName(AudioEffectType::Delay);
        case AudioNodeKind::Gain:        return nous::audio::DisplayName(AudioEffectType::Gain);
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Node-kind <-> effect-type mapping
// ---------------------------------------------------------------------------

bool AudioGraphEditor::IsEffect(AudioNodeKind kind)
{
    return kind != AudioNodeKind::AudioSource && kind != AudioNodeKind::AudioOutput;
}

AudioEffectType AudioGraphEditor::ToEffectType(AudioNodeKind kind)
{
    switch (kind)
    {
        case AudioNodeKind::LowPass:  return AudioEffectType::LowPass;
        case AudioNodeKind::HighPass: return AudioEffectType::HighPass;
        case AudioNodeKind::Delay:    return AudioEffectType::Delay;
        case AudioNodeKind::Gain:     return AudioEffectType::Gain;
        default:                      return AudioEffectType::Gain;   // unreachable for anchors
    }
}

AudioNodeKind AudioGraphEditor::FromEffectType(AudioEffectType type)
{
    switch (type)
    {
        case AudioEffectType::LowPass:  return AudioNodeKind::LowPass;
        case AudioEffectType::HighPass: return AudioNodeKind::HighPass;
        case AudioEffectType::Delay:    return AudioNodeKind::Delay;
        case AudioEffectType::Gain:     return AudioNodeKind::Gain;
    }
    return AudioNodeKind::Gain;
}

// Stub — replaced with the live-preview push in Task 5.
void AudioGraphEditor::OnParamEdited(const AudioNode& /*node*/, int /*paramIndex*/)
{
}

ImU32 AudioGraphEditor::GetCategoryHeaderColor(AudioNodeCategory category)
{
    switch (category)
    {
        case AudioNodeCategory::Source:  return IM_COL32(178,  60,  60, 230);
        case AudioNodeCategory::Output:  return IM_COL32( 55,  95, 180, 230);
        case AudioNodeCategory::Effect:  return IM_COL32( 55, 145,  85, 230);
        case AudioNodeCategory::Utility: return IM_COL32(180, 145,  50, 230);
    }
    return IM_COL32(120, 120, 120, 230);
}

ImColor AudioGraphEditor::GetCategoryBorderColor(AudioNodeCategory category)
{
    switch (category)
    {
        case AudioNodeCategory::Source:  return ImColor(220, 110, 110, 255);
        case AudioNodeCategory::Output:  return ImColor(110, 140, 220, 255);
        case AudioNodeCategory::Effect:  return ImColor(110, 200, 140, 255);
        case AudioNodeCategory::Utility: return ImColor(220, 185,  95, 255);
    }
    return ImColor(180, 180, 180, 255);
}

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

AudioGraphEditor::AudioGraphEditor(const char* title, EditorContext* context, bool start_open) :
    IEditorWindow(title, context, nullptr, start_open)
{
}

AudioGraphEditor::~AudioGraphEditor()
{
    if (m_context)
    {
        ed::DestroyEditor(m_context);
        m_context = nullptr;
    }
}

void AudioGraphEditor::Init()
{
    ed::Config config;
    // Persistence intentionally disabled — view state (positions / zoom /
    // selection) will be embedded inside the future ResourceAudioGraph asset
    // instead of written to a stray sidecar JSON. nullptr makes the library
    // skip both the LoadSettings read and the SaveSettings write on Destroy.
    config.SettingsFile   = nullptr;
    config.CanvasSizeMode = ed::CanvasSizeMode::CenterOnly;
    m_context = ed::CreateEditor(&config);

    // Seed with a default Source -> Output pair so the canvas isn't empty.
    m_nodes.push_back(MakeNode(AudioNodeKind::AudioSource, ImVec2( 40.0f, 80.0f)));
    m_nodes.push_back(MakeNode(AudioNodeKind::AudioOutput, ImVec2(280.0f, 80.0f)));
}

bool AudioGraphEditor::Begin(bool& outVisible)
{
    // See feedback_imgui_node_editor_host_setup: WindowPadding=0 around the
    // host window's ImGui::Begin is required for the canvas to size correctly.
    // Pop immediately after Begin so popups/menus opened from this window
    // (which inherit current style) get the default padding, not (0,0).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool result = IEditorWindow::Begin(outVisible);
    ImGui::PopStyleVar();

    return result;
}

void AudioGraphEditor::End()
{
    IEditorWindow::End();
}

// ---------------------------------------------------------------------------
// Node factory
// ---------------------------------------------------------------------------

AudioNode AudioGraphEditor::MakeNode(AudioNodeKind kind, ImVec2 position)
{
    AudioNode node;
    node.id              = ed::NodeId(NextID());
    node.kind            = kind;
    node.name            = GetKindLabel(kind);
    node.position        = position;
    node.positionPending = true;

    auto addInput = [&](const char* name)
    {
        node.inputs.push_back({ ed::PinId(NextID()), ed::PinKind::Input, name });
    };
    auto addOutput = [&](const char* name)
    {
        node.outputs.push_back({ ed::PinId(NextID()), ed::PinKind::Output, name });
    };

    if (IsEffect(kind))
        node.params = nous::audio::DefaultParams(ToEffectType(kind));

    switch (kind)
    {
        case AudioNodeKind::AudioSource: addOutput("Out"); break;
        case AudioNodeKind::AudioOutput: addInput("In");   break;
        default:                                            // all effects: 1 in, 1 out
            addInput("In");
            addOutput("Out");
            break;
    }
    return node;
}

void AudioGraphEditor::SpawnNode(AudioNodeKind kind)
{
    // Offset each successive spawn so nodes don't stack on top of each other.
    const float stride = 24.0f;
    const int   ring   = m_spawnStrideCount % 8;
    const ImVec2 jitter{ stride * ring, stride * ring };
    m_nodes.push_back(MakeNode(kind, ImVec2(m_spawnPosition.x + jitter.x,
                                            m_spawnPosition.y + jitter.y)));
    ++m_spawnStrideCount;
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void AudioGraphEditor::DrawMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("Add Node"))
    {
        if (ImGui::MenuItem("Audio Source")) SpawnNode(AudioNodeKind::AudioSource);
        if (ImGui::MenuItem("Audio Output")) SpawnNode(AudioNodeKind::AudioOutput);
        ImGui::Separator();
        // Effect palette is registry-driven — grows automatically as effects are added.
        for (AudioEffectType t : nous::audio::k_allEffects)
        {
            if (ImGui::MenuItem(nous::audio::DisplayName(t)))
                SpawnNode(FromEffectType(t));
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Graph"))
    {
        const bool hasContent = !m_nodes.empty();
        if (ImGui::MenuItem("Zoom to fit", nullptr, false, hasContent))
        {
            ed::SetCurrentEditor(m_context);
            ed::NavigateToContent(0.25f);
            ed::SetCurrentEditor(nullptr);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear graph", nullptr, false, hasContent))
        {
            m_nodes.clear();
            m_links.clear();
            m_spawnStrideCount = 0;
        }
        ImGui::EndMenu();
    }

    // Right-aligned status text
    char status[64];
    std::snprintf(status, sizeof(status), "%zu nodes  /  %zu links",
                  m_nodes.size(), m_links.size());
    const float textWidth = ImGui::CalcTextSize(status).x;
    const float available = ImGui::GetContentRegionAvail().x;
    if (available > textWidth + 12.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available - textWidth - 12.0f));
    ImGui::TextDisabled("%s", status);

    ImGui::EndMenuBar();
}

// ---------------------------------------------------------------------------
// Node drawing
// ---------------------------------------------------------------------------

void AudioGraphEditor::DrawNode(AudioNode& node)
{
    const AudioNodeCategory category    = GetCategory(node.kind);
    const ImU32             headerColor = GetCategoryHeaderColor(category);
    const ImColor           borderColor = GetCategoryBorderColor(category);

    if (node.positionPending)
    {
        ed::SetNodePosition(node.id, node.position);
        node.positionPending = false;
    }

    ed::PushStyleColor(ed::StyleColor_NodeBorder, borderColor);
    ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 2.0f);
    ed::PushStyleVar(ed::StyleVar_NodeRounding,    6.0f);
    ed::PushStyleVar(ed::StyleVar_NodePadding,     ImVec4(8, 4, 8, 8));

    float headerHeight = 0.0f;

    ed::BeginNode(node.id);
    {
        // --- Header text (its height drives the colored strip behind it) ---
        ImGui::BeginGroup();
        ImGui::TextUnformatted(node.name.c_str());
        ImGui::EndGroup();
        headerHeight = ImGui::GetItemRectSize().y;

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // --- Two-column pin layout: inputs left, outputs right ---
        ImGui::BeginGroup();
        for (auto& pin : node.inputs)
        {
            ed::BeginPin(pin.id, ed::PinKind::Input);
            ImGui::Text("->  %s", pin.name.c_str());
            ed::EndPin();
        }
        if (node.inputs.empty())
            ImGui::Dummy(ImVec2(40.0f, 0.0f));
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 28.0f);

        ImGui::BeginGroup();
        for (auto& pin : node.outputs)
        {
            ed::BeginPin(pin.id, ed::PinKind::Output);
            ImGui::Text("%s  ->", pin.name.c_str());
            ed::EndPin();
        }
        if (node.outputs.empty())
            ImGui::Dummy(ImVec2(40.0f, 0.0f));
        ImGui::EndGroup();

        // --- Inline parameter widgets (effect nodes only), drawn from the schema ---
        if (IsEffect(node.kind) && !node.params.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushID(static_cast<int>(node.id.Get()));   // disambiguate identical param labels across nodes
            ImGui::PushItemWidth(120.0f);

            std::span<const AudioEffectParamDesc> schema = nous::audio::Params(ToEffectType(node.kind));
            for (size_t i = 0; i < schema.size() && i < node.params.size(); ++i)
            {
                const AudioEffectParamDesc& p = schema[i];
                if (ImGui::DragFloat(p.name, &node.params[i],
                                     (p.max - p.min) * 0.005f, p.min, p.max, "%.3f"))
                {
                    node.params[i] = std::clamp(node.params[i], p.min, p.max);
                    m_dirty = true;
                    OnParamEdited(node, static_cast<int>(i));   // push live to preview if active (Task 5)
                }
            }

            ImGui::PopItemWidth();
            ImGui::PopID();
        }
    }
    ed::EndNode();

    ed::PopStyleVar(3);
    ed::PopStyleColor(1);

    // --- Paint a colored header strip behind the top of the node ---
    // After EndNode, ImGui::GetItemRect{Min,Max} returns the node's screen bounds.
    // GetNodeBackgroundDrawList draws beneath node contents but above the body fill.
    const ImVec2 nodeMin = ImGui::GetItemRectMin();
    const ImVec2 nodeMax = ImGui::GetItemRectMax();
    if (ImDrawList* bg = ed::GetNodeBackgroundDrawList(node.id))
    {
        const float stripHeight = headerHeight + 10.0f;
        bg->AddRectFilled(
            ImVec2(nodeMin.x, nodeMin.y),
            ImVec2(nodeMax.x, nodeMin.y + stripHeight),
            headerColor,
            ed::GetStyle().NodeRounding,
            ImDrawFlags_RoundCornersTop);
    }
}

// ---------------------------------------------------------------------------
// Link create / delete handling
// ---------------------------------------------------------------------------

void AudioGraphEditor::HandleCreateAndDelete()
{
    if (ed::BeginCreate())
    {
        ed::PinId inputPinID, outputPinID;
        if (ed::QueryNewLink(&inputPinID, &outputPinID))
        {
            if (inputPinID && outputPinID && ed::AcceptNewItem())
            {
                m_links.push_back({ ed::LinkId(NextID()), inputPinID, outputPinID });
                ed::Link(m_links.back().linkID, m_links.back().inputID, m_links.back().outputID);
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
        ed::LinkId deletedLinkID;
        while (ed::QueryDeletedLink(&deletedLinkID))
        {
            if (ed::AcceptDeletedItem())
            {
                for (auto it = m_links.begin(); it != m_links.end(); ++it)
                {
                    if (it->linkID == deletedLinkID)
                    {
                        m_links.erase(it);
                        break;
                    }
                }
            }
        }

        ed::NodeId deletedNodeID;
        while (ed::QueryDeletedNode(&deletedNodeID))
        {
            if (ed::AcceptDeletedItem())
            {
                for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it)
                {
                    if (it->id == deletedNodeID)
                    {
                        m_nodes.erase(it);
                        break;
                    }
                }
            }
        }
    }
    ed::EndDelete();
}

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

void AudioGraphEditor::DrawCanvas()
{
    ed::SetCurrentEditor(m_context);
    ed::Begin("AudioGraph", ImVec2(0.0f, 0.0f));
    {
        // Refresh spawn-from-menu target each frame to be in the middle of the
        // visible canvas area, so menu-spawned nodes appear where the user is
        // looking instead of at canvas-origin.
        const ImVec2 screenMin = ImGui::GetWindowPos();
        const ImVec2 screenMax = ImVec2(screenMin.x + ImGui::GetWindowSize().x,
                                        screenMin.y + ImGui::GetWindowSize().y);
        const ImVec2 screenMid{ (screenMin.x + screenMax.x) * 0.5f,
                                (screenMin.y + screenMax.y) * 0.5f };
        m_spawnPosition = ed::ScreenToCanvas(screenMid);

        for (auto& node : m_nodes)
            DrawNode(node);

        for (auto& link : m_links)
            ed::Link(link.linkID, link.inputID, link.outputID);

        // The node editor's built-in Delete-key handler only fires when its
        // CanAcceptUserInput() is true (focused AND hovered). In this host
        // setup that's flaky, so detect Delete ourselves and queue deletions
        // explicitly — BeginDelete/QueryDeleted* below will then pick them up.
        // Accept Delete, Backspace, and Numpad-Decimal as "delete selection".
        // On some keyboard layouts (notably Spanish ISO via SDL) the physical
        // "Supr" key reports as SDL_SCANCODE_KP_PERIOD → ImGuiKey_KeypadDecimal
        // instead of ImGuiKey_Delete, so we listen for all three.
        const bool deletePressed =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            (ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal,  false) ||
             ImGui::IsKeyPressed(ImGuiKey_Pause,          false));
        if (deletePressed)
        {
            ed::NodeId selectedNodes[64];
            const int  selectedNodeCount = ed::GetSelectedNodes(
                selectedNodes, IM_ARRAYSIZE(selectedNodes));
            for (int i = 0; i < selectedNodeCount; ++i)
                ed::DeleteNode(selectedNodes[i]);

            ed::LinkId selectedLinks[128];
            const int  selectedLinkCount = ed::GetSelectedLinks(
                selectedLinks, IM_ARRAYSIZE(selectedLinks));
            for (int i = 0; i < selectedLinkCount; ++i)
                ed::DeleteLink(selectedLinks[i]);
        }

        HandleCreateAndDelete();
    }
    ed::End();

    // Zoom-to-fit on first open. Deferred until frame 2 because on frame 0
    // nodes are only SetNodePosition'd; their measured size (driven by ImGui
    // layout) is finalized one frame later. Calling NavigateToContent before
    // node bounds exist produces a wrong fit.
    if (!m_initialFitDone && !m_nodes.empty() && m_framesSinceOpen >= 1)
    {
        ed::NavigateToContent(0.0f);
        m_initialFitDone = true;
    }

    ed::SetCurrentEditor(nullptr);

    ++m_framesSinceOpen;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void AudioGraphEditor::DrawContent()
{
    if (!m_context)
        return;

    DrawMenuBar();

    // Thin info row above the canvas. This also serves a load-bearing role:
    // imgui-node-editor's canvas-rect calculations require a real measured
    // item in the host window before ed::Begin (see feedback memory note).
    ImGui::Text(" RMB-drag: pan   |   Mouse wheel: zoom   |   Supr: remove selected");
    ImGui::Separator();

    DrawCanvas();
}

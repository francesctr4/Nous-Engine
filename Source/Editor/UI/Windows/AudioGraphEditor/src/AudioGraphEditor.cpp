#include "Editor/UI/Windows/AudioGraphEditor/include/AudioGraphEditor.h"

#include <imgui.h>

namespace ed = ax::NodeEditor;

// ---------------------------------------------------------------------------
// Category metadata
// ---------------------------------------------------------------------------

AudioNodeCategory AudioGraphEditor::GetCategory(AudioNodeKind kind)
{
    switch (kind)
    {
        case AudioNodeKind::AudioSource:    return AudioNodeCategory::Source;
        case AudioNodeKind::AudioOutput:    return AudioNodeCategory::Output;
        case AudioNodeKind::Equalizer:
        case AudioNodeKind::LowPassFilter:
        case AudioNodeKind::HighPassFilter:
        case AudioNodeKind::Reverb:
        case AudioNodeKind::Delay:          return AudioNodeCategory::Effect;
        case AudioNodeKind::Gain:
        case AudioNodeKind::Mixer:
        case AudioNodeKind::Splitter:       return AudioNodeCategory::Utility;
    }
    return AudioNodeCategory::Utility;
}

const char* AudioGraphEditor::GetKindLabel(AudioNodeKind kind)
{
    switch (kind)
    {
        case AudioNodeKind::AudioSource:    return "Audio Source";
        case AudioNodeKind::AudioOutput:    return "Audio Output";
        case AudioNodeKind::Equalizer:      return "Equalizer";
        case AudioNodeKind::LowPassFilter:  return "Low-Pass Filter";
        case AudioNodeKind::HighPassFilter: return "High-Pass Filter";
        case AudioNodeKind::Reverb:         return "Reverb";
        case AudioNodeKind::Delay:          return "Delay";
        case AudioNodeKind::Gain:           return "Gain";
        case AudioNodeKind::Mixer:          return "Mixer";
        case AudioNodeKind::Splitter:       return "Splitter";
    }
    return "Unknown";
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

    switch (kind)
    {
        case AudioNodeKind::AudioSource:    addOutput("Out"); break;
        case AudioNodeKind::AudioOutput:    addInput("In");   break;

        case AudioNodeKind::Equalizer:
        case AudioNodeKind::LowPassFilter:
        case AudioNodeKind::HighPassFilter:
        case AudioNodeKind::Reverb:
        case AudioNodeKind::Delay:
        case AudioNodeKind::Gain:
            addInput("In");
            addOutput("Out");
            break;

        case AudioNodeKind::Mixer:
            addInput("In 1");
            addInput("In 2");
            addInput("In 3");
            addOutput("Out");
            break;

        case AudioNodeKind::Splitter:
            addInput("In");
            addOutput("Out 1");
            addOutput("Out 2");
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
        if (ImGui::BeginMenu("Sources"))
        {
            if (ImGui::MenuItem("Audio Source"))     SpawnNode(AudioNodeKind::AudioSource);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Outputs"))
        {
            if (ImGui::MenuItem("Audio Output"))     SpawnNode(AudioNodeKind::AudioOutput);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Effects"))
        {
            if (ImGui::MenuItem("Equalizer"))        SpawnNode(AudioNodeKind::Equalizer);
            if (ImGui::MenuItem("Low-Pass Filter"))  SpawnNode(AudioNodeKind::LowPassFilter);
            if (ImGui::MenuItem("High-Pass Filter")) SpawnNode(AudioNodeKind::HighPassFilter);
            if (ImGui::MenuItem("Reverb"))           SpawnNode(AudioNodeKind::Reverb);
            if (ImGui::MenuItem("Delay"))            SpawnNode(AudioNodeKind::Delay);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Utilities"))
        {
            if (ImGui::MenuItem("Gain"))     SpawnNode(AudioNodeKind::Gain);
            if (ImGui::MenuItem("Mixer"))    SpawnNode(AudioNodeKind::Mixer);
            if (ImGui::MenuItem("Splitter")) SpawnNode(AudioNodeKind::Splitter);
            ImGui::EndMenu();
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

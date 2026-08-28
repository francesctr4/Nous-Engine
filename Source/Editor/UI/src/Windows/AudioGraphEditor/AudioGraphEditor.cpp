#include <EditorUI/AudioGraphEditor.h>
#include <EngineCore/Casts.h>

#include <EditorCore/EditorContext.h>
#include <Logger/Logger.h>
#include <ModuleAudio/ModuleAudio.h>
#include <ModuleResourceManager/ModuleResourceManager.h>
#include <ModuleScene/ModuleScene.h>
#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>
#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>
#include <ResourceManager/Types/ResourceAudioGraph/ImporterAudioGraph.h>

#include <imgui.h>
#include <imgui_internal.h>   // BeginDragDropTargetCustom + ImRect (full-canvas drop target)
#include <glm/glm.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <unordered_map>

namespace ed = ax::NodeEditor;

namespace
{
// Walk an ASSETS_BROWSER_ITEMS drag-drop payload (a run of null-terminated paths) and
// return the first path whose extension matches one of `exts`, or "" if none match.
std::string FirstDroppedAsset(const ImGuiPayload* payload,
                              std::initializer_list<const char*> exts)
{
    const char* data = static_cast<const char*>(payload->Data);
    const char* end  = data + payload->DataSize;
    while (data < end && *data)
    {
        std::string path(data);
        data += path.size() + 1;
        const std::string ext = std::filesystem::path(path).extension().string();
        for (const char* e : exts)
            if (ext == e)
                return path;
    }
    return {};
}
} // namespace

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

// Push a live param edit to an active preview. Patches the single param in place via
// SetEffectParam (no node teardown → no audible gap) for everything the backend can
// update live. Delay-time (param 0) is baked into the buffer length at node init, so
// it — and any change we can't map to the live chain — falls back to a full rebuild.
void AudioGraphEditor::OnParamEdited(const AudioNode& node, int paramIndex)
{
    if (!m_previewVoice)
        return;
    ModuleAudio* audio = AudioModule();
    if (!audio)
        return;

    const bool delayTime = IsEffect(node.kind)
                        && ToEffectType(node.kind) == AudioEffectType::Delay
                        && paramIndex == 0;

    int effectIndex = -1;
    if (!delayTime && m_previewChain)
        effectIndex = EffectIndexInChain(node);

    if (delayTime || !m_previewChain || effectIndex < 0)
    {
        RebuildPreviewChain();   // structural / unmappable change — full rebuild
        return;
    }

    if (paramIndex >= 0 && paramIndex < static_cast<int>(node.params.size()))
        audio->SetEffectParam(m_previewChain, effectIndex, paramIndex, node.params[paramIndex]);
}

int AudioGraphEditor::EffectIndexInChain(const AudioNode& node)
{
    // Effects are emitted into the chain in Source→Output path order — the same order
    // ChainOrderedNodes walks — so counting effects up to `node` yields its chain index.
    std::vector<AudioNode*> ordered = ChainOrderedNodes();
    int idx = 0;
    for (AudioNode* n : ordered)
    {
        if (!IsEffect(n->kind))
            continue;
        if (n->id == node.id)
            return idx;
        ++idx;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// In-editor preview (Task 5)
// ---------------------------------------------------------------------------

ModuleAudio* AudioGraphEditor::AudioModule() const
{
    ModuleScene* scene = editorContext ? editorContext->GetScene() : nullptr;
    return scene ? scene->GetAudio() : nullptr;
}

void AudioGraphEditor::PreviewStop()
{
    ModuleAudio* audio = AudioModule();
    if (audio)
    {
        if (m_previewChain) audio->DestroyEffectChain(m_previewChain);   // chain hangs off the voice — release first
        if (m_previewVoice) audio->DestroySound(m_previewVoice);
    }
    m_previewChain = nullptr;
    m_previewVoice = nullptr;
}

void AudioGraphEditor::PreviewPlay()
{
    ModuleAudio* audio = AudioModule();
    if (!audio || !m_previewClip)
        return;
    PreviewStop();

    m_previewVoice = audio->CreateSound(m_previewClip, AudioBus::Master);
    if (!m_previewVoice)
        return;

    audio->SetSoundSpatializationEnabled(m_previewVoice, false);   // 2D audition
    audio->SetSoundLooping(m_previewVoice, true);                  // loop so live edits stay audible

    RebuildPreviewChain();          // build the DSP chain from the live graph
    audio->StartSound(m_previewVoice);
}

void AudioGraphEditor::RebuildPreviewChain()
{
    ModuleAudio* audio = AudioModule();
    if (!audio || !m_previewVoice)
        return;

    if (m_previewChain) { audio->DestroyEffectChain(m_previewChain); m_previewChain = nullptr; }

    AudioGraphDesc desc;
    if (LinearizeCurrent(desc) && !desc.empty())
        m_previewChain = audio->CreateEffectChain(m_previewVoice, desc, AudioBus::Master);
}

void AudioGraphEditor::SetPreviewClip(const std::string& audioPath)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm)
        return;

    if (ResourceBase* r = rm->CreateResource(audioPath))
    {
        PreviewStop();   // drop a new clip → drop the old audition
        // CreateResource added a fresh ref for this assignment, so always release
        // the previously-held one (even if it's the same clip → balanced refcount).
        if (m_previewClip)
            rm->UnloadResource(m_previewClip->GetUID());
        m_previewClip = down_cast<ResourceAudio*>(r);
    }
}

// ---------------------------------------------------------------------------
// Asset binding: linearize / load / save / new / open
// ---------------------------------------------------------------------------

ModuleResourceManager* AudioGraphEditor::ResourceManager() const
{
    return editorContext ? editorContext->GetResourceManager() : nullptr;
}

void AudioGraphEditor::BuildProxies(std::vector<nous::audio_editor::LinNode>& outNodes,
                                    std::vector<nous::audio_editor::LinLink>& outLinks) const
{
    using namespace nous::audio_editor;
    outNodes.clear();
    outLinks.clear();

    for (const AudioNode& n : m_nodes)
    {
        const LinNodeKind k = n.kind == AudioNodeKind::AudioSource ? LinNodeKind::Source
                            : n.kind == AudioNodeKind::AudioOutput ? LinNodeKind::Output
                                                                   : LinNodeKind::Effect;
        const AudioEffectType et = IsEffect(n.kind) ? ToEffectType(n.kind) : AudioEffectType::Gain;
        outNodes.push_back({ static_cast<uint64_t>(n.id.Get()), k, et, n.params });
    }

    // Resolve each link's endpoints to node ids by pin ownership. imgui-node-editor
    // does not guarantee which of inputID/outputID is the output pin, so check both
    // pin lists: a node owning an *output* pin is the "from", an *input* pin the "to".
    for (const AudioGraphLink& l : m_links)
    {
        uint64_t fromNode = 0;
        uint64_t toNode   = 0;
        for (const AudioNode& n : m_nodes)
        {
            for (const AudioNodePin& p : n.outputs)
                if (p.id == l.inputID || p.id == l.outputID) fromNode = static_cast<uint64_t>(n.id.Get());
            for (const AudioNodePin& p : n.inputs)
                if (p.id == l.inputID || p.id == l.outputID) toNode = static_cast<uint64_t>(n.id.Get());
        }
        if (fromNode && toNode)
            outLinks.push_back({ fromNode, toNode });
    }
}

bool AudioGraphEditor::LinearizeCurrent(AudioGraphDesc& outDesc) const
{
    std::vector<nous::audio_editor::LinNode> ln;
    std::vector<nous::audio_editor::LinLink> ll;
    BuildProxies(ln, ll);
    return nous::audio_editor::Linearize(ln, ll, outDesc);
}

std::vector<AudioNode*> AudioGraphEditor::ChainOrderedNodes()
{
    std::vector<nous::audio_editor::LinNode> ln;
    std::vector<nous::audio_editor::LinLink> ll;
    BuildProxies(ln, ll);

    std::vector<uint64_t> order;
    if (!nous::audio_editor::LinearizeOrder(ln, ll, order))
        return {};

    std::unordered_map<uint64_t, AudioNode*> byId;
    for (AudioNode& n : m_nodes)
        byId[static_cast<uint64_t>(n.id.Get())] = &n;

    std::vector<AudioNode*> result;
    result.reserve(order.size());
    for (uint64_t id : order)
    {
        auto it = byId.find(id);
        if (it == byId.end())
            return {};
        result.push_back(it->second);
    }
    return result;
}

void AudioGraphEditor::LoadFromResource(ResourceAudioGraph* graph)
{
    // Release the previously-open asset's ref before swapping.
    if (ModuleResourceManager* rm = ResourceManager())
        if (m_graph && m_graph != graph)
            rm->UnloadResource(m_graph->GetUID());

    m_nodes.clear();
    m_links.clear();
    m_spawnStrideCount = 0;
    m_initialFitDone   = false;
    m_framesSinceOpen  = 0;
    m_graph            = graph;
    m_dirty            = false;

    if (!graph)
        return;

    // Stored positions follow [source, effects..., output]; use them if the count
    // matches, otherwise auto-place left→right.
    const size_t expected = graph->effects.size() + 2;
    const bool   usePos   = graph->editorPositions.size() == expected;
    auto posAt = [&](size_t i, float fallbackX) -> ImVec2
    {
        if (usePos) return ImVec2(graph->editorPositions[i].x, graph->editorPositions[i].y);
        return ImVec2(fallbackX, 80.0f);
    };

    m_nodes.push_back(MakeNode(AudioNodeKind::AudioSource, posAt(0, 40.0f)));

    float x = 240.0f;
    for (size_t i = 0; i < graph->effects.size(); ++i)
    {
        const AudioEffectDesc& e = graph->effects[i];
        AudioNode fx = MakeNode(FromEffectType(e.type), posAt(i + 1, x));
        fx.params = e.params;                                   // asset values override defaults
        fx.params.resize(nous::audio::Params(e.type).size());  // keep schema-sized
        m_nodes.push_back(std::move(fx));
        x += 200.0f;
    }

    m_nodes.push_back(MakeNode(AudioNodeKind::AudioOutput, posAt(expected - 1, x)));

    // Chain them: source → fx0 → ... → fxN → output.
    for (size_t i = 0; i + 1 < m_nodes.size(); ++i)
    {
        const AudioNodePin& outPin = m_nodes[i].outputs.front();
        const AudioNodePin& inPin  = m_nodes[i + 1].inputs.front();
        m_links.push_back({ ed::LinkId(NextID()), inPin.id, outPin.id });
    }
}

bool AudioGraphEditor::SaveToOpenAsset()
{
    if (!m_graph)
        return false;

    // Walk the chain once; effects[] and editorPositions stay parallel by construction.
    std::vector<AudioNode*> ordered = ChainOrderedNodes();
    if (ordered.empty())
    {
        // Refuse to save a half-wired graph. Keep m_dirty set (the '*' marker stays) so
        // the unsaved state is still visible, and tell the user why nothing was written.
        NOUS_WARN("[AudioGraphEditor] Save skipped: '%s' has no complete Audio Source -> Audio Output path. "
                  "Wire the chain before saving.",
                  m_graph->GetName().c_str());
        return false;
    }

    m_graph->effects.clear();
    m_graph->editorPositions.clear();

    ed::SetCurrentEditor(m_context);
    for (AudioNode* n : ordered)
    {
        const ImVec2 p = ed::GetNodePosition(n->id);
        n->position = p;
        m_graph->editorPositions.push_back(glm::vec2(p.x, p.y));
        if (IsEffect(n->kind))
            m_graph->effects.push_back({ ToEffectType(n->kind), n->params });
    }
    ed::SetCurrentEditor(nullptr);

    m_graph->generation += 1;   // activates CAudioSource live-rebuild (Layer 2)

    ImporterAudioGraph::WriteAudioGraphToFile(*m_graph, m_graph->GetAssetsPath());
    ImporterAudioGraph::WriteAudioGraphToFile(*m_graph, m_graph->GetLibraryPath());
    m_dirty = false;
    return true;
}

void AudioGraphEditor::NewAsset(const std::string& name)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm)
        return;

    const std::string dir = editorContext->GetAssetsBrowserDirectory();

    // Auto-suffix on collision so the chosen name never clobbers an existing file.
    std::string path = dir + "/" + name + ".nafx";
    for (int i = 1; std::filesystem::exists(path); ++i)
    {
        if (i > 9999)
            return;  // sanity bail
        path = dir + "/" + name + "_" + std::to_string(i) + ".nafx";
    }

    if (!ImporterAudioGraph::CreateNewAudioGraphFile(path))
        return;
    rm->ImportFile(path);   // mirror to Library + assign a UID
    if (ResourceBase* r = rm->CreateResource(path))
        LoadFromResource(down_cast<ResourceAudioGraph*>(r));
}

void AudioGraphEditor::DrawNewAssetPopup()
{
    // OpenPopup must run at window scope, not inside the menu bar (the menu's
    // ID stack mis-scopes the popup). The menu item only sets the flag.
    if (m_showNewAssetPopup)
    {
        ImGui::OpenPopup("New Audio Graph");
        std::strncpy(m_newAssetName, "NewAudioGraph", sizeof(m_newAssetName) - 1);
        m_newAssetName[sizeof(m_newAssetName) - 1] = '\0';
        m_showNewAssetPopup = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Audio Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create in: %s", editorContext->GetAssetsBrowserDirectory().c_str());
        ImGui::Spacing();
        ImGui::Text("Graph Name:");
        ImGui::SetNextItemWidth(300.0f);
        const bool enterPressed = ImGui::InputText("##NafxName", m_newAssetName, IM_ARRAYSIZE(m_newAssetName),
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const bool nameEmpty = std::strlen(m_newAssetName) == 0;
        ImGui::BeginDisabled(nameEmpty);
        if (ImGui::Button("Create", ImVec2(120, 0)) || (enterPressed && !nameEmpty))
        {
            NewAsset(m_newAssetName);
            m_newAssetName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_newAssetName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AudioGraphEditor::OpenAsset(const std::string& nafxPath)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm)
        return;
    if (ResourceBase* r = rm->CreateResource(nafxPath))
        LoadFromResource(down_cast<ResourceAudioGraph*>(r));
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
    // Tear down the preview voice/chain first so no audio outlives the window
    // (would leak the NOUS_NEW'd ma_sound and trip the ShutdownMemory leak-abort).
    PreviewStop();

    // Release the open asset's + preview clip's refs.
    if (ModuleResourceManager* rm = ResourceManager())
    {
        if (m_graph)       rm->UnloadResource(m_graph->GetUID());
        if (m_previewClip) rm->UnloadResource(m_previewClip->GetUID());
    }
    m_graph       = nullptr;
    m_previewClip = nullptr;

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

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New"))
            m_showNewAssetPopup = true;  // deferred: OpenPopup at window scope (see DrawNewAssetPopup)
        if (ImGui::MenuItem("Save", nullptr, false, m_graph != nullptr))
            SaveToOpenAsset();
        ImGui::EndMenu();
    }

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
    char status[160];
    std::snprintf(status, sizeof(status), "%s%s  |  %zu nodes  /  %zu links",
                  m_graph ? m_graph->GetName().c_str() : "(no asset)",
                  m_dirty ? " *" : "",
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
        // "Supr" / Delete as "delete selection". This host setup never receives a
        // plain ImGuiKey_Delete: on the Spanish ISO layout (via SDL) the physical
        // "Supr" key reports as SDL_SCANCODE_KP_PERIOD → ImGuiKey_KeypadDecimal,
        // and the upper-row variant comes through as ImGuiKey_Pause — so listen for
        // those two rather than ImGuiKey_Delete.
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
// Toolbar (preview controls + help)
// ---------------------------------------------------------------------------

void AudioGraphEditor::DrawToolbar()
{
    ModuleAudio* audio   = AudioModule();
    const bool   playing = m_previewVoice && audio && audio->IsSoundPlaying(m_previewVoice);

    // Play / Stop toggle. Disabled (greyed) until a preview clip is assigned.
    if (playing)
    {
        if (ImGui::Button("Stop ##preview"))
            PreviewStop();
    }
    else
    {
        ImGui::BeginDisabled(m_previewClip == nullptr);
        if (ImGui::Button("Play ##preview"))
            PreviewPlay();
        ImGui::EndDisabled();
    }

    // Clip slot — labelled button doubling as a drop target for a .wav/.ogg.
    ImGui::SameLine();
    ImGui::TextUnformatted("Preview clip:");
    ImGui::SameLine();
    ImGui::Button(m_previewClip ? m_previewClip->GetName().c_str()
                                : "(none — drop a .wav / .ogg)");
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
        {
            const std::string clip = FirstDroppedAsset(payload, { ".wav", ".ogg" });
            if (!clip.empty())
                SetPreviewClip(clip);
        }
        ImGui::EndDragDropTarget();
    }

    // Right-aligned help marker — folds the pan/zoom/delete hints into a tooltip
    // so the toolbar stays uncluttered.
    const char* help = "(?)";
    const float helpW = ImGui::CalcTextSize(help).x;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - helpW - ImGui::GetStyle().FramePadding.x);
    ImGui::TextDisabled("%s", help);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("RMB-drag: pan\n"
                          "Mouse wheel: zoom\n"
                          "Supr: remove selected\n"
                          "\n"
                          "Drop a .nafx onto the canvas to open it.\n"
                          "Drop a .wav / .ogg to set the preview clip.");
}

// ---------------------------------------------------------------------------
// Full-canvas drop target (routes by extension)
// ---------------------------------------------------------------------------

void AudioGraphEditor::HandleCanvasDrop(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    // BeginDragDropTargetCustom registers a drop zone over an arbitrary rect without
    // an owning item, and only becomes live while an external payload is being dragged
    // — so it never steals the clicks the node editor needs for panning / selection.
    const ImRect rect(canvasMin, canvasMax);
    if (!ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID("AudioGraphCanvasDrop")))
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
    {
        // .nafx wins over a clip if both appear in the same multi-select drop.
        if (std::string nafx = FirstDroppedAsset(payload, { ".nafx" }); !nafx.empty())
            OpenAsset(nafx);
        else if (std::string clip = FirstDroppedAsset(payload, { ".wav", ".ogg" }); !clip.empty())
            SetPreviewClip(clip);
    }
    ImGui::EndDragDropTarget();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void AudioGraphEditor::DrawContent()
{
    if (!m_context)
        return;

    DrawMenuBar();
    DrawNewAssetPopup();  // modal name prompt opened by File ▸ New

    // The toolbar is also load-bearing: imgui-node-editor's canvas-rect calculation
    // requires a real measured item in the host window before ed::Begin (its buttons
    // satisfy that — see feedback_imgui_node_editor_host_setup).
    DrawToolbar();
    ImGui::Separator();

    // Capture the canvas rect before ed::Begin advances the cursor, so the whole
    // remaining window area can be wired as a drop target after the canvas draws.
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 avail     = ImGui::GetContentRegionAvail();
    const ImVec2 canvasMax = ImVec2(canvasMin.x + avail.x, canvasMin.y + avail.y);

    DrawCanvas();

    HandleCanvasDrop(canvasMin, canvasMax);
}

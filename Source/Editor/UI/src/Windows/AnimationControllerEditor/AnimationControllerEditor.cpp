#include <EditorUI/AnimationControllerEditor.h>
#include <EngineCore/Casts.h>

// ParameterDecl::type is a raw uint8_t and Controller.h only forward-declares
// AnimParameters -- the ENUM those bytes mean lives here, which is what lets the
// pure layer carry declarations without depending on the blackboard.
#include <AnimationSystem/AnimParameters.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/GameObject.h>
#include <EditorCore/EditorContext.h>
#include <Logger/Logger.h>
#include <ModuleResourceManager/ModuleResourceManager.h>
#include <ModuleScene/ModuleScene.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceAnimationController/ImporterAnimationController.h>
#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>

#include <imgui.h>
#include <imgui_internal.h>   // BeginDragDropTargetCustom + ImRect (full-canvas drop target)
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

namespace ed = ax::NodeEditor;
namespace as = nous::engine::animation_system;

namespace
{
    // Walk an ASSETS_BROWSER_ITEMS payload (a run of null-terminated paths) and return
    // the first whose extension matches. Copied from AudioGraphEditor -- the payload
    // shape is the Assets Browser's, not this window's.
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

    // The literal a transition endpoint uses on disk for Any State, and therefore a
    // name no real state may take -- a state called "AnyState" would round-trip as
    // the sentinel and quietly become a transition from everywhere.
    constexpr const char* c_anyStateName = "AnyState";

    // Comparators are offered per TYPE, not as one flat list. A Float with IsTrue or
    // a Bool with Greater are conditions that can never be satisfied, and an editor
    // that lets you author one is an editor that ships them.
    std::vector<as::ConditionComparator> ComparatorsFor(const as::AnimParamType type)
    {
        switch (type)
        {
            case as::AnimParamType::Bool:
                return { as::ConditionComparator::IsTrue, as::ConditionComparator::IsFalse };
            case as::AnimParamType::Trigger:
                return { as::ConditionComparator::TriggerSet };
            case as::AnimParamType::Float:
            default:
                return { as::ConditionComparator::Greater, as::ConditionComparator::Less };
        }
    }

    const char* ComparatorLabel(const as::ConditionComparator c)
    {
        switch (c)
        {
            case as::ConditionComparator::Greater:    return "greater than";
            case as::ConditionComparator::Less:       return "less than";
            case as::ConditionComparator::IsTrue:     return "is true";
            case as::ConditionComparator::IsFalse:    return "is false";
            case as::ConditionComparator::TriggerSet: return "is set";
        }
        return "?";
    }

    bool TakesValue(const as::ConditionComparator c)
    {
        return c == as::ConditionComparator::Greater || c == as::ConditionComparator::Less;
    }

    std::string WarningText(const nous::anim_editor::ValidationWarning& w)
    {
        switch (w.kind)
        {
            case nous::anim_editor::WarningKind::StateHasNoClip:
                return "State '" + w.subject + "' has no clip -- it will play nothing.";
            case nous::anim_editor::WarningKind::NoDefaultState:
                return "No default state. The animator will fall back to the first one.";
            case nous::anim_editor::WarningKind::UndeclaredParameter:
                return "Condition names undeclared parameter '" + w.subject + "'.";
            case nous::anim_editor::WarningKind::UnconditionalTransition:
                return "Transition out of '" + w.subject +
                       "' has no conditions and no exit time -- it fires immediately.";
            case nous::anim_editor::WarningKind::UnreachableState:
                return "State '" + w.subject + "' cannot be reached from the default state.";
        }
        return {};
    }
}

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

AnimationControllerEditor::AnimationControllerEditor(const char* title, EditorContext* context,
                                                     bool start_open) :
    IEditorWindow(title, context, nullptr, start_open)
{
}

AnimationControllerEditor::~AnimationControllerEditor()
{
    ReleaseNodeClips();

    if (ModuleResourceManager* rm = ResourceManager())
        if (m_controller)
            rm->UnloadResource(m_controller->GetUID());

    m_controller = nullptr;

    if (m_context)
    {
        ed::DestroyEditor(m_context);
        m_context = nullptr;
    }
}

void AnimationControllerEditor::Init()
{
    ed::Config config;
    // Persistence off: node positions live in the .nctrl's own editor block, so a
    // sidecar json would be a second, stale source of the same thing.
    config.SettingsFile   = nullptr;
    config.CanvasSizeMode = ed::CanvasSizeMode::CenterOnly;
    m_context = ed::CreateEditor(&config);

    // Any State exists from the first frame and is never deleted: it is part of the
    // graph's vocabulary, not something the user adds.
    m_nodes.push_back(MakeNode(ControllerNodeKind::AnyState, ImVec2(40.0f, 40.0f)));
}

bool AnimationControllerEditor::Begin(bool& outVisible)
{
    // WindowPadding=0 around the host Begin is required for the canvas to size
    // correctly. Popped immediately so popups and menus opened from this window --
    // which inherit the live style stack -- get the default padding, not (0,0).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool result = IEditorWindow::Begin(outVisible);
    ImGui::PopStyleVar();

    return result;
}

void AnimationControllerEditor::End()
{
    IEditorWindow::End();
}

// ---------------------------------------------------------------------------
// Nodes
// ---------------------------------------------------------------------------

ControllerNode AnimationControllerEditor::MakeNode(const ControllerNodeKind kind, const ImVec2 position)
{
    ControllerNode node;
    node.id              = ed::NodeId(NextID());
    node.kind            = kind;
    node.position        = position;
    node.positionPending = true;

    node.input  = { ed::PinId(NextID()), ed::PinKind::Input };
    node.output = { ed::PinId(NextID()), ed::PinKind::Output };

    if (kind == ControllerNodeKind::State)
        node.state.name = UniqueStateName("New State", node.id);

    return node;
}

void AnimationControllerEditor::SpawnState()
{
    const float  stride = 28.0f;
    const int    ring   = m_spawnStrideCount % 8;
    const ImVec2 jitter{ stride * ring, stride * ring };

    m_nodes.push_back(MakeNode(ControllerNodeKind::State,
                               ImVec2(m_spawnPosition.x + jitter.x, m_spawnPosition.y + jitter.y)));
    ++m_spawnStrideCount;

    // The first state authored becomes the default, so a fresh controller does not
    // open with a NoDefaultState warning the user has to go hunting for.
    if (m_defaultStateName.empty())
        m_defaultStateName = m_nodes.back().state.name;

    m_dirty = true;
}

ControllerNode* AnimationControllerEditor::FindNodeByOutputPin(const ed::PinId pin)
{
    for (ControllerNode& n : m_nodes)
        if (n.output.id == pin)
            return &n;
    return nullptr;
}

ControllerNode* AnimationControllerEditor::FindNodeByInputPin(const ed::PinId pin)
{
    for (ControllerNode& n : m_nodes)
        if (n.input.id == pin)
            return &n;
    return nullptr;
}

ControllerNode* AnimationControllerEditor::FindNodeById(const ed::NodeId id)
{
    for (ControllerNode& n : m_nodes)
        if (n.id == id)
            return &n;
    return nullptr;
}

const ControllerNode* AnimationControllerEditor::AnyStateNode() const
{
    for (const ControllerNode& n : m_nodes)
        if (n.kind == ControllerNodeKind::AnyState)
            return &n;
    return nullptr;
}

std::string AnimationControllerEditor::UniqueStateName(const std::string& desired,
                                                       const ed::NodeId exclude) const
{
    // Transitions are addressed by state NAME on disk, so two states sharing one is
    // not a cosmetic clash: FindState returns the first, and every link to the other
    // silently retargets on the next load.
    const std::string base = desired.empty() ? std::string("State") : desired;

    const auto taken = [this, exclude](const std::string& candidate) -> bool
    {
        if (candidate == c_anyStateName)
            return true;

        for (const ControllerNode& n : m_nodes)
            if (n.kind == ControllerNodeKind::State && n.id != exclude && n.state.name == candidate)
                return true;

        return false;
    };

    if (!taken(base))
        return base;

    for (int i = 1; i < 10000; ++i)
    {
        const std::string candidate = base + " " + std::to_string(i);
        if (!taken(candidate))
            return candidate;
    }
    return base;
}

// ---------------------------------------------------------------------------
// Clip references
// ---------------------------------------------------------------------------

ModuleResourceManager* AnimationControllerEditor::ResourceManager() const
{
    return editorContext ? editorContext->GetResourceManager() : nullptr;
}

const CAnimator* AnimationControllerEditor::WatchedAnimator() const
{
    if (!m_controller || !editorContext)
        return nullptr;

    const ModuleScene* scene = editorContext->GetScene();
    if (!scene || scene->IsStopped())
        return nullptr;   // a stopped scene has no current state to show

    // Exactly one selected object. With several, there is no single answer to "which
    // state is active", and picking the primary silently would make the highlight
    // mean something different from what the user sees selected.
    if (scene->selectedGameObjects.size() != 1)
        return nullptr;

    GameObject go = scene->selectedGameObjects.front();
    if (!go.IsValid() || !go.HasComponent<CAnimator>())
        return nullptr;

    const CAnimator& animator = go.GetComponent<CAnimator>();

    // Only when it is playing THIS asset. Highlighting against a different
    // controller would tint whichever states happened to share a name.
    return animator.controller == m_controller ? &animator : nullptr;
}

void AnimationControllerEditor::SetNodeClip(ControllerNode& node, const std::string& nanimPath)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm)
        return;

    ResourceAnimation* acquired = nullptr;
    if (!nanimPath.empty())
    {
        // Null-tested BEFORE the cast: down_cast asserts on null, and a path that
        // does not resolve is an ordinary outcome here, not a type error.
        if (ResourceBase* r = rm->CreateResource(nanimPath))
            acquired = down_cast<ResourceAnimation*>(r);
    }

    // ACQUIRE, THEN RELEASE -- the third place in this feature where a slot changes
    // owner, and the same rule as the other two. Releasing first would let the count
    // transiently hit 0 and queue a spurious eviction of the very clip being kept;
    // and the release must be unconditional, because re-resolving the SAME clip
    // finds it resident and only increments, so a `previous != acquired` guard never
    // fires in the common case.
    ResourceAnimation* previous = node.clip;
    node.clip = acquired;

    if (previous)
        rm->UnloadResource(previous->GetUID());

    m_dirty = true;
}

void AnimationControllerEditor::ReleaseNodeClips()
{
    ModuleResourceManager* rm = ResourceManager();

    for (ControllerNode& n : m_nodes)
    {
        if (n.clip && rm)
            rm->UnloadResource(n.clip->GetUID());
        n.clip = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Canvas <-> graph
// ---------------------------------------------------------------------------

void AnimationControllerEditor::BuildProxies(std::vector<nous::anim_editor::BuildNode>& outNodes,
                                             std::vector<nous::anim_editor::BuildLink>& outLinks) const
{
    outNodes.clear();
    outLinks.clear();

    int stateIndex = 0;
    for (const ControllerNode& n : m_nodes)
    {
        nous::anim_editor::BuildNode proxy;
        proxy.id    = n.id.Get();
        proxy.kind  = n.kind == ControllerNodeKind::AnyState
                          ? nous::anim_editor::BuildNodeKind::AnyState
                          : nous::anim_editor::BuildNodeKind::State;
        proxy.state = n.state;

        if (n.kind == ControllerNodeKind::State)
        {
            // Mirrors what ResolveClips produces: clipIndex is the STATE index when a
            // clip resolved and -1 when it did not. Validate only asks which of the
            // two it is, but keeping the same convention means the warnings the user
            // sees match what the runtime will actually do with the saved asset.
            proxy.state.clipIndex = n.clip ? stateIndex : -1;
            ++stateIndex;
        }

        outNodes.push_back(std::move(proxy));
    }

    // The pin ids are what tie a link to its endpoints, so they are resolved back to
    // node ids here -- BuildGraph knows nothing about pins.
    for (const ControllerLink& l : m_links)
    {
        const ControllerNode* from = nullptr;
        const ControllerNode* to   = nullptr;

        for (const ControllerNode& n : m_nodes)
        {
            if (n.output.id == l.outputID) from = &n;
            if (n.input.id  == l.inputID)  to   = &n;
        }

        if (!from || !to)
            continue;

        nous::anim_editor::BuildLink proxy;
        proxy.fromNodeId = from->id.Get();
        proxy.toNodeId   = to->id.Get();
        proxy.transition = l.transition;

        outLinks.push_back(std::move(proxy));
    }
}

void AnimationControllerEditor::LoadFromResource(ResourceAnimationController* controller)
{
    ModuleResourceManager* rm = ResourceManager();

    ReleaseNodeClips();

    // ACQUIRE-THEN-RELEASE, UNCONDITIONALLY. The caller (OpenAsset / NewAsset) has
    // already acquired `controller`, so the previous one is released here -- held
    // aside first, because it may BE `controller`.
    //
    // The obvious `m_controller != controller` guard is wrong and leaks in exactly
    // the common case, which is re-dropping the asset that is already open:
    // CreateResource finds it resident and only INCREMENTS, so a change-detecting
    // release never fires and the count climbs one per drop. Same rule, same reason,
    // as ImporterMaterial's texture slots and ResolveClips' clip slots.
    ResourceAnimationController* previousController = m_controller;

    m_nodes.clear();
    m_links.clear();
    m_parameters.clear();
    m_defaultStateName.clear();
    m_spawnStrideCount = 0;
    m_initialFitDone   = false;
    m_framesSinceOpen  = 0;
    m_controller       = controller;
    m_dirty            = false;

    // AFTER the assignment, so the count cannot transiently hit 0 and queue a
    // spurious eviction of the very controller being opened.
    if (rm && previousController)
        rm->UnloadResource(previousController->GetUID());

    // Any State first, so it exists even for an empty controller.
    m_nodes.push_back(MakeNode(ControllerNodeKind::AnyState, ImVec2(40.0f, 40.0f)));

    if (!controller)
        return;

    const as::ControllerGraph& graph = controller->graph;

    m_parameters = graph.parameters;

    if (graph.IsValidState(graph.defaultState))
        m_defaultStateName = graph.states[graph.defaultState].name;

    // State index -> node index in m_nodes, so transitions can find both ends. Not a
    // map from name: two states can transiently share a name in a hand-edited asset,
    // and the index is what the graph's transitions actually carry.
    std::vector<size_t> nodeOfState;
    nodeOfState.reserve(graph.states.size());

    for (size_t i = 0; i < graph.states.size(); ++i)
    {
        const ImVec2 pos = i < controller->editorPositions.size()
                               ? ImVec2(controller->editorPositions[i].x, controller->editorPositions[i].y)
                               : ImVec2(260.0f + 200.0f * static_cast<float>(i % 4),
                                        80.0f  + 140.0f * static_cast<float>(i / 4));

        ControllerNode node = MakeNode(ControllerNodeKind::State, pos);
        node.state = graph.states[i];

        nodeOfState.push_back(m_nodes.size());
        m_nodes.push_back(std::move(node));

        // Resolved from the AUTHORED slot, not from controller->clips, so a state
        // whose clip the controller failed to resolve keeps its broken reference
        // visible instead of silently becoming an unbound state on the next save.
        if (i < controller->clipSlots.size() && !controller->clipSlots[i].assetPath.empty())
            SetNodeClip(m_nodes.back(), controller->clipSlots[i].assetPath);
    }

    for (const as::ControllerTransition& t : graph.transitions)
    {
        const ControllerNode* fromNode = nullptr;

        if (t.fromState == as::ControllerGraph::c_anyState)
            fromNode = AnyStateNode();
        else if (graph.IsValidState(t.fromState))
            fromNode = &m_nodes[nodeOfState[static_cast<size_t>(t.fromState)]];

        // A transition whose endpoint did not resolve (the importer writes -1 for a
        // name that names no state) is DROPPED rather than drawn against a wrong
        // node. The asset keeps it until the next save; the canvas cannot show it.
        if (!fromNode || !graph.IsValidState(t.toState))
            continue;

        const ControllerNode& toNode = m_nodes[nodeOfState[static_cast<size_t>(t.toState)]];

        ControllerLink link;
        link.linkID     = ed::LinkId(NextID());
        link.inputID    = toNode.input.id;
        link.outputID   = fromNode->output.id;
        link.transition = t;

        m_links.push_back(std::move(link));
    }

    // Loading set clips through SetNodeClip, which marks the window dirty. Nothing
    // has been EDITED, so clear it -- otherwise every open shows unsaved changes.
    m_dirty = false;
}

bool AnimationControllerEditor::SaveToOpenAsset()
{
    if (!m_controller)
        return false;

    std::vector<nous::anim_editor::BuildNode> proxyNodes;
    std::vector<nous::anim_editor::BuildLink> proxyLinks;
    BuildProxies(proxyNodes, proxyLinks);

    as::ControllerGraph graph = nous::anim_editor::BuildGraph(proxyNodes, proxyLinks);

    graph.parameters  = m_parameters;
    graph.defaultState = m_defaultStateName.empty() ? -1 : graph.FindState(m_defaultStateName);

    m_controller->graph = std::move(graph);

    // clips / clipSlots / editorPositions are all parallel to states and are rebuilt
    // wholesale, in the SAME convention ResolveClips uses (clipIndex == state index
    // when a clip resolved, -1 otherwise). A different convention here would load
    // back as a different graph.
    //
    // THE CONTROLLER OWNS ITS OWN REFERENCES, separately from this window's. They
    // were acquired by ResolveClips, so simply clearing the vector would leak one per
    // clip on every save -- and would leave Evict later releasing counts the
    // controller never took. Held aside and drained after the refill, the same
    // structure ResolveClips uses and for the same two reasons: unconditional,
    // because nothing compares old against new; and after, because a clip present in
    // both sets must never transiently reach 0.
    ModuleResourceManager* rm = ResourceManager();

    std::vector<ResourceAnimation*> previousClips;
    previousClips.swap(m_controller->clips);

    m_controller->clipSlots.clear();
    m_controller->editorPositions.clear();

    ed::SetCurrentEditor(m_context);

    int stateIndex = 0;
    for (ControllerNode& n : m_nodes)
    {
        if (n.kind != ControllerNodeKind::State)
            continue;

        const ImVec2 p = ed::GetNodePosition(n.id);
        n.position = p;
        m_controller->editorPositions.push_back(glm::vec2(p.x, p.y));

        ControllerClipSlot slot;
        if (n.clip)
        {
            slot.assetPath   = n.clip->GetAssetsPath();
            slot.libraryPath = n.clip->GetLibraryPath();
            slot.uid         = n.clip->GetUID();
        }
        m_controller->clipSlots.push_back(std::move(slot));

        // A reference OF THE CONTROLLER'S, taken here rather than sharing the
        // window's: the two have independent lifetimes -- closing this window must
        // not leave a live controller pointing at clips nobody holds.
        ResourceAnimation* owned = nullptr;
        if (n.clip && rm)
        {
            if (ResourceBase* r = rm->CreateResource(n.clip->GetAssetsPath()))
                owned = down_cast<ResourceAnimation*>(r);
        }

        m_controller->clips.push_back(owned);
        m_controller->graph.states[static_cast<size_t>(stateIndex)].clipIndex = owned ? stateIndex : -1;

        ++stateIndex;
    }

    ed::SetCurrentEditor(nullptr);

    // Drained only now, so a clip present in both the old and new sets never drops
    // to 0 between the release and the re-acquire.
    for (ResourceAnimation* old : previousClips)
        if (old && rm)
            rm->UnloadResource(old->GetUID());

    if (!ImporterAnimationController::WriteControllerToFile(*m_controller, m_controller->GetAssetsPath()))
    {
        NOUS_WARN("[AnimationControllerEditor] Failed to write '%s'.",
                  m_controller->GetAssetsPath().c_str());
        return false;
    }
    ImporterAnimationController::WriteControllerToFile(*m_controller, m_controller->GetLibraryPath());

    // AFTER the write, and by the editor rather than by WriteControllerToFile: the
    // round-trip test writes without perturbing generation, and a playing CAnimator
    // compares it every frame to decide whether to rebuild (Task 9).
    m_controller->generation += 1;

    m_dirty = false;
    return true;
}

void AnimationControllerEditor::NewAsset(const std::string& name)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm)
        return;

    const std::string dir = editorContext->GetAssetsBrowserDirectory();

    // Auto-suffix on collision so the chosen name never clobbers an existing asset.
    std::string path = dir + "/" + name + ".nctrl";
    for (int i = 1; std::filesystem::exists(path); ++i)
    {
        if (i > 9999)
            return;
        path = dir + "/" + name + "_" + std::to_string(i) + ".nctrl";
    }

    if (!ImporterAnimationController::CreateNewControllerFile(path))
        return;

    rm->ImportFile(path);   // mirror into Library/ and mint a UID

    if (ResourceBase* r = rm->CreateResource(path))
        LoadFromResource(down_cast<ResourceAnimationController*>(r));
}

void AnimationControllerEditor::OpenAsset(const std::string& nctrlPath)
{
    ModuleResourceManager* rm = ResourceManager();
    if (!rm)
        return;

    if (ResourceBase* r = rm->CreateResource(nctrlPath))
        LoadFromResource(down_cast<ResourceAnimationController*>(r));
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void AnimationControllerEditor::DrawMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New"))
            m_showNewAssetPopup = true;   // deferred: OpenPopup at window scope
        if (ImGui::MenuItem("Save", nullptr, false, m_controller != nullptr))
            SaveToOpenAsset();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Add"))
    {
        if (ImGui::MenuItem("State"))
            SpawnState();
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
        ImGui::EndMenu();
    }

    char status[192];
    std::snprintf(status, sizeof(status), "%s%s  |  %zu states  /  %zu transitions",
                  m_controller ? m_controller->GetName().c_str() : "(no asset)",
                  m_dirty ? " *" : "",
                  m_nodes.empty() ? 0u : m_nodes.size() - 1u,   // Any State is not a state
                  m_links.size());

    const float textWidth = ImGui::CalcTextSize(status).x;
    const float available = ImGui::GetContentRegionAvail().x;
    if (available > textWidth + 12.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available - textWidth - 12.0f));
    ImGui::TextDisabled("%s", status);

    ImGui::EndMenuBar();
}

void AnimationControllerEditor::DrawNewAssetPopup()
{
    if (m_showNewAssetPopup)
    {
        ImGui::OpenPopup("New Animation Controller");
        std::snprintf(m_newAssetName, sizeof(m_newAssetName), "%s", "NewController");
        m_showNewAssetPopup = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Animation Controller", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create in: %s", editorContext->GetAssetsBrowserDirectory().c_str());
        ImGui::Spacing();
        ImGui::Text("Controller Name:");
        ImGui::SetNextItemWidth(300.0f);
        const bool enterPressed = ImGui::InputText("##NctrlName", m_newAssetName, IM_ARRAYSIZE(m_newAssetName),
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

// ---------------------------------------------------------------------------
// Left panel
// ---------------------------------------------------------------------------

void AnimationControllerEditor::DrawParametersSection()
{
    ImGui::TextDisabled("PARAMETERS");

    for (size_t i = 0; i < m_parameters.size(); ++i)
    {
        as::ParameterDecl& decl = m_parameters[i];
        ImGui::PushID(static_cast<int>(i));

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s", decl.name.c_str());

        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputText("##pname", buf, sizeof(buf)))
            RenameParameter(i, buf);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);

        // Order matches AnimParamType's declaration -- the combo indexes the enum by
        // value, the same contract that has already bitten CAudioSource's attenuation
        // combo and CAnimator's root motion one. Keep the two in step.
        int typeIndex = static_cast<int>(decl.type);
        static const char* const c_typeNames[] = { "Float", "Bool", "Trigger" };
        if (ImGui::Combo("##ptype", &typeIndex, c_typeNames, 3))
        {
            decl.type = static_cast<uint8_t>(typeIndex);
            m_dirty   = true;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
        {
            m_parameters.erase(m_parameters.begin() + static_cast<long>(i));
            m_dirty = true;
            ImGui::PopID();
            break;   // the indices below have shifted; redraw next frame
        }

        // A Trigger has no default -- it is set or it is not -- so there is nothing
        // to offer, and offering a greyed control would imply otherwise.
        const auto type = static_cast<as::AnimParamType>(decl.type);
        if (type == as::AnimParamType::Float)
        {
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::DragFloat("default##pdef", &decl.defaultValue, 0.01f))
                m_dirty = true;
        }
        else if (type == as::AnimParamType::Bool)
        {
            bool on = decl.defaultValue != 0.0f;
            if (ImGui::Checkbox("default##pdef", &on))
            {
                decl.defaultValue = on ? 1.0f : 0.0f;
                m_dirty = true;
            }
        }

        ImGui::PopID();
    }

    if (ImGui::SmallButton("Add Parameter"))
    {
        as::ParameterDecl decl;
        decl.name = "param" + std::to_string(m_parameters.size());
        m_parameters.push_back(std::move(decl));
        m_dirty = true;
    }
}

void AnimationControllerEditor::RenameParameter(const size_t index, const std::string& newName)
{
    if (index >= m_parameters.size())
        return;

    const std::string previous = m_parameters[index].name;
    if (previous == newName)
        return;

    m_parameters[index].name = newName;

    // Every condition naming the old parameter follows it. Leaving them behind would
    // be reported by the UndeclaredParameter warning -- correct, and exactly the kind
    // of correct that is avoidable.
    for (ControllerLink& link : m_links)
        for (as::ControllerCondition& c : link.transition.conditions)
            if (c.parameter == previous)
                c.parameter = newName;

    m_dirty = true;
}

void AnimationControllerEditor::DrawStateDetails(ControllerNode& node)
{
    ImGui::TextDisabled("STATE");

    // Captured BEFORE the edit: after it, node.state.name is the new name and the
    // comparison would never match, so renaming the default state would quietly
    // leave m_defaultStateName pointing at a name no state has.
    const bool wasDefault = (m_defaultStateName == node.state.name);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s", node.state.name.c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##sname", buf, sizeof(buf)))
    {
        // Uniqueness enforced on every keystroke rather than on commit: transitions
        // are addressed by NAME on disk, so a duplicate is not a cosmetic clash but a
        // link that silently retargets on the next load.
        node.state.name = UniqueStateName(buf, node.id);

        if (wasDefault)
            m_defaultStateName = node.state.name;

        m_dirty = true;
    }

    // Clip slot -- a labelled button doubling as a .nanim drop target.
    ImGui::TextDisabled("Clip");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Button(node.clip ? node.clip->GetName().c_str() : "(none -- drop a .nanim)",
                  ImVec2(-1.0f, 0.0f));

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
            if (const std::string nanim = FirstDroppedAsset(payload, ".nanim"); !nanim.empty())
                SetNodeClip(node, nanim);
        ImGui::EndDragDropTarget();
    }

    if (node.clip && ImGui::SmallButton("Clear Clip"))
        SetNodeClip(node, {});

    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat("Speed##sspeed", &node.state.speed, 0.01f, -4.0f, 4.0f, "%.2f"))
        m_dirty = true;

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("MULTIPLIES the clip's own authored speed.\n"
                          "It never replaces it -- two things claiming to be\n"
                          "'the speed' is what the per-clip setting replaced.");

    // A Float parameter that further scales this state's rate: the cheap stand-in for
    // the blend tree that is out of scope. Only Floats are offered -- a Bool or a
    // Trigger as a rate multiplier has no meaning.
    {
        std::vector<const char*> names{ "(none)" };
        for (const as::ParameterDecl& p : m_parameters)
            if (static_cast<as::AnimParamType>(p.type) == as::AnimParamType::Float)
                names.push_back(p.name.c_str());

        int current = 0;
        for (int i = 1; i < static_cast<int>(names.size()); ++i)
            if (node.state.speedParameter == names[static_cast<size_t>(i)])
                current = i;

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("Speed Param##sspeedparam", &current, names.data(),
                         static_cast<int>(names.size())))
        {
            node.state.speedParameter = current == 0 ? std::string() : names[static_cast<size_t>(current)];
            m_dirty = true;
        }
    }

    // Inherit is enumerator 0 and IS offered here, unlike on the component: on a
    // state it is the meaningful default -- "whatever the character is set to".
    static const char* const c_rootMotionNames[] = { "Inherit", "Baked", "Applied", "In Place" };
    int rootMotionIndex = std::clamp(node.state.rootMotion, 0, 3);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("Root Motion##srm", &rootMotionIndex, c_rootMotionNames, 4))
    {
        node.state.rootMotion = rootMotionIndex;
        m_dirty = true;
    }

    ImGui::Spacing();

    const bool isDefault = (m_defaultStateName == node.state.name);
    ImGui::BeginDisabled(isDefault);
    if (ImGui::Button(isDefault ? "Is Default" : "Set as Default", ImVec2(-1.0f, 0.0f)))
    {
        m_defaultStateName = node.state.name;
        m_dirty = true;
    }
    ImGui::EndDisabled();
}

void AnimationControllerEditor::DrawTransitionDetails(ControllerLink& link)
{
    ImGui::TextDisabled("TRANSITION");

    as::ControllerTransition& t = link.transition;

    if (ImGui::Checkbox("Has Exit Time", &t.hasExitTime))
        m_dirty = true;

    if (t.hasExitTime)
    {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("Exit Time##texit", &t.exitTime, 0.01f, 0.0f, 1.0f, "%.2f"))
            m_dirty = true;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Normalized progress through the SOURCE state's clip.\n"
                              "Measured against the state being LEFT.");
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat("Duration##tdur", &t.duration, 0.01f, 0.0f, 5.0f, "%.2f"))
        m_dirty = true;

    ImGui::Spacing();
    ImGui::TextDisabled("Conditions (all must hold)");

    int removeCondition = -1;
    for (size_t i = 0; i < t.conditions.size(); ++i)
    {
        as::ControllerCondition& c = t.conditions[i];
        ImGui::PushID(static_cast<int>(i));

        // Parameter combo over DECLARED names only. A free text field is what makes a
        // typo reach the runtime, where an IsFalse on an unknown name is always
        // satisfied and the transition fires forever.
        std::vector<const char*> names;
        for (const as::ParameterDecl& p : m_parameters)
            names.push_back(p.name.c_str());

        int current = -1;
        for (int n = 0; n < static_cast<int>(names.size()); ++n)
            if (c.parameter == names[static_cast<size_t>(n)])
                current = n;

        ImGui::SetNextItemWidth(-1.0f);
        if (!names.empty() &&
            ImGui::Combo("##cparam", &current, names.data(), static_cast<int>(names.size())))
        {
            c.parameter = names[static_cast<size_t>(current)];

            // The comparator is typed, so a parameter change can leave one that can
            // never be satisfied. Snap it to the new type's first valid comparator.
            const auto type = static_cast<as::AnimParamType>(m_parameters[static_cast<size_t>(current)].type);
            const std::vector<as::ConditionComparator> valid = ComparatorsFor(type);
            if (std::find(valid.begin(), valid.end(), c.comparator) == valid.end())
                c.comparator = valid.front();

            m_dirty = true;
        }

        if (names.empty())
            ImGui::TextDisabled("(declare a parameter first)");

        // Comparators filtered by the parameter's declared type.
        if (current >= 0)
        {
            const auto type = static_cast<as::AnimParamType>(m_parameters[static_cast<size_t>(current)].type);
            const std::vector<as::ConditionComparator> valid = ComparatorsFor(type);

            std::vector<const char*> labels;
            labels.reserve(valid.size());
            for (const as::ConditionComparator v : valid)
                labels.push_back(ComparatorLabel(v));

            int comparatorIndex = 0;
            for (int n = 0; n < static_cast<int>(valid.size()); ++n)
                if (valid[static_cast<size_t>(n)] == c.comparator)
                    comparatorIndex = n;

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##ccmp", &comparatorIndex, labels.data(), static_cast<int>(labels.size())))
            {
                c.comparator = valid[static_cast<size_t>(comparatorIndex)];
                m_dirty = true;
            }

            if (TakesValue(c.comparator))
            {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::DragFloat("##cval", &c.value, 0.01f))
                    m_dirty = true;
            }
        }

        if (ImGui::SmallButton("Remove Condition"))
            removeCondition = static_cast<int>(i);

        ImGui::Separator();
        ImGui::PopID();
    }

    if (removeCondition >= 0)
    {
        t.conditions.erase(t.conditions.begin() + removeCondition);
        m_dirty = true;
    }

    if (ImGui::SmallButton("Add Condition"))
    {
        as::ControllerCondition c;
        if (!m_parameters.empty())
        {
            c.parameter = m_parameters.front().name;
            c.comparator = ComparatorsFor(static_cast<as::AnimParamType>(m_parameters.front().type)).front();
        }
        t.conditions.push_back(std::move(c));
        m_dirty = true;
    }

    // ORDER IS THE PRIORITY: the evaluator walks transitions in order and the first
    // satisfied one wins. So these buttons move the entry in m_links itself, not just
    // in a display list -- reordering only the view would change nothing at runtime
    // while appearing to.
    ImGui::Spacing();
    ImGui::TextDisabled("Priority among this state's transitions");

    const auto indexOf = [this](const ControllerLink& target) -> long
    {
        for (size_t i = 0; i < m_links.size(); ++i)
            if (m_links[i].linkID == target.linkID)
                return static_cast<long>(i);
        return -1;
    };

    const long self = indexOf(link);

    // The neighbour to swap with is the nearest link sharing this one's SOURCE pin --
    // links from other states sit between them in m_links and must not be disturbed.
    const auto neighbour = [this, self](const int direction) -> long
    {
        if (self < 0) return -1;
        const ed::PinId source = m_links[static_cast<size_t>(self)].outputID;

        for (long i = self + direction; i >= 0 && i < static_cast<long>(m_links.size()); i += direction)
            if (m_links[static_cast<size_t>(i)].outputID == source)
                return i;

        return -1;
    };

    const long up   = neighbour(-1);
    const long down = neighbour(+1);

    // `moved` guards the second button: a swap invalidates `self`, `down` AND the
    // `link` reference this function was handed, so acting on both in one frame would
    // move some other state's transition.
    bool moved = false;

    ImGui::BeginDisabled(up < 0);
    if (ImGui::SmallButton("Move Up"))
    {
        std::swap(m_links[static_cast<size_t>(self)], m_links[static_cast<size_t>(up)]);
        m_dirty = true;
        moved   = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(down < 0 || moved);
    if (ImGui::SmallButton("Move Down") && !moved)
    {
        std::swap(m_links[static_cast<size_t>(self)], m_links[static_cast<size_t>(down)]);
        m_dirty = true;
    }
    ImGui::EndDisabled();
}

void AnimationControllerEditor::DrawSelectionSection()
{
    ed::SetCurrentEditor(m_context);

    ed::NodeId selectedNodes[8];
    ed::LinkId selectedLinks[8];
    const int  nodeCount = ed::GetSelectedNodes(selectedNodes, IM_ARRAYSIZE(selectedNodes));
    const int  linkCount = ed::GetSelectedLinks(selectedLinks, IM_ARRAYSIZE(selectedLinks));

    ed::SetCurrentEditor(nullptr);

    if (nodeCount == 1 && linkCount == 0)
    {
        if (ControllerNode* node = FindNodeById(selectedNodes[0]))
        {
            if (node->kind == ControllerNodeKind::AnyState)
            {
                ImGui::TextDisabled("ANY STATE");
                ImGui::TextWrapped("Transitions drawn from here are evaluated from EVERY state, "
                                   "before that state's own. It has no settings of its own.");
                return;
            }

            DrawStateDetails(*node);
            return;
        }
    }

    if (linkCount == 1 && nodeCount == 0)
    {
        for (ControllerLink& link : m_links)
        {
            if (link.linkID == selectedLinks[0])
            {
                DrawTransitionDetails(link);
                return;
            }
        }
    }

    ImGui::TextDisabled("SELECTION");
    ImGui::TextWrapped("%s", nodeCount + linkCount > 1
                                 ? "Select a single state or transition to edit it."
                                 : "Select a state or a transition.");
}

void AnimationControllerEditor::DrawWarningsSection()
{
    std::vector<nous::anim_editor::BuildNode> proxyNodes;
    std::vector<nous::anim_editor::BuildLink> proxyLinks;
    BuildProxies(proxyNodes, proxyLinks);

    as::ControllerGraph graph = nous::anim_editor::BuildGraph(proxyNodes, proxyLinks);
    graph.parameters   = m_parameters;
    graph.defaultState = m_defaultStateName.empty() ? -1 : graph.FindState(m_defaultStateName);

    // Recomputed every frame rather than cached. The graph is tens of nodes, and a
    // cache would need invalidating on every edit path in this file -- the warnings
    // going stale is precisely how the panel stops being believed.
    const std::vector<nous::anim_editor::ValidationWarning> warnings = nous::anim_editor::Validate(graph);

    ImGui::TextDisabled("WARNINGS (%zu)", warnings.size());

    if (warnings.empty())
    {
        ImGui::TextDisabled("None.");
        return;
    }

    for (const nous::anim_editor::ValidationWarning& w : warnings)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s", WarningText(w).c_str());
        ImGui::PopStyleColor();
    }
}

void AnimationControllerEditor::DrawLeftPanel()
{
    DrawParametersSection();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    DrawSelectionSection();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    DrawWarningsSection();
}

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

void AnimationControllerEditor::DrawNode(ControllerNode& node, const std::string_view activeState)
{
    const bool isAnyState = node.kind == ControllerNodeKind::AnyState;
    const bool isDefault  = !isAnyState && node.state.name == m_defaultStateName;

    // Matched BY NAME, never by index: while this window holds unsaved edits its
    // canvas and the resource the animator is playing disagree about what lives at
    // any given index, so an index match would tint the wrong node exactly when the
    // user is mid-edit and most wants to trust it.
    const bool isActive = !isAnyState && !activeState.empty() && activeState == node.state.name;

    const ImU32 headerColor = isAnyState ? IM_COL32(150,  90, 170, 230)
                            : isDefault  ? IM_COL32( 55, 145,  85, 230)
                                         : IM_COL32( 60,  90, 140, 230);

    // The active border overrides the others rather than blending with them: while
    // the scene plays, "where is the animator right now" is the only thing anyone is
    // looking at, and a default state that is also active must read as active.
    const ImColor borderColor = isActive   ? ImColor(255, 205,  90, 255)
                              : isAnyState ? ImColor(195, 140, 215, 255)
                              : isDefault  ? ImColor(110, 200, 140, 255)
                                           : ImColor(120, 155, 210, 255);

    if (node.positionPending)
    {
        ed::SetNodePosition(node.id, node.position);
        node.positionPending = false;
    }

    ed::PushStyleColor(ed::StyleColor_NodeBorder, borderColor);
    ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, isActive ? 4.0f : 2.0f);
    ed::PushStyleVar(ed::StyleVar_NodeRounding,    6.0f);
    ed::PushStyleVar(ed::StyleVar_NodePadding,     ImVec4(8, 4, 8, 8));

    float headerHeight = 0.0f;

    ed::BeginNode(node.id);
    {
        ImGui::BeginGroup();
        ImGui::TextUnformatted(isAnyState ? c_anyStateName : node.state.name.c_str());
        ImGui::EndGroup();
        headerHeight = ImGui::GetItemRectSize().y;

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        ImGui::BeginGroup();
        // Any State has NO input pin drawn: it is a source of transitions and
        // nothing can transition into it. Not drawing the pin is what makes that
        // unauthorable, rather than relying on the rejection in HandleCreateAndDelete.
        if (!isAnyState)
        {
            ed::BeginPin(node.input.id, ed::PinKind::Input);
            ImGui::TextUnformatted("->  in");
            ed::EndPin();
        }
        else
        {
            ImGui::Dummy(ImVec2(40.0f, 0.0f));
        }
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 28.0f);

        ImGui::BeginGroup();
        ed::BeginPin(node.output.id, ed::PinKind::Output);
        ImGui::TextUnformatted("out  ->");
        ed::EndPin();
        ImGui::EndGroup();

        if (!isAnyState)
        {
            // The clip name under the state name, dimmed. A state with no clip plays
            // nothing and is otherwise indistinguishable on the canvas from one that
            // does, so the absence is spelled out rather than left blank.
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            if (node.clip)
                ImGui::TextDisabled("%s", node.clip->GetName().c_str());
            else
                ImGui::TextDisabled("(no clip)");
        }
    }
    ed::EndNode();

    ed::PopStyleVar(3);
    ed::PopStyleColor(1);

    const ImVec2 nodeMin = ImGui::GetItemRectMin();
    const ImVec2 nodeMax = ImGui::GetItemRectMax();
    if (ImDrawList* bg = ed::GetNodeBackgroundDrawList(node.id))
    {
        const float stripHeight = headerHeight + 10.0f;
        bg->AddRectFilled(ImVec2(nodeMin.x, nodeMin.y),
                          ImVec2(nodeMax.x, nodeMin.y + stripHeight),
                          headerColor,
                          ed::GetStyle().NodeRounding,
                          ImDrawFlags_RoundCornersTop);
    }
}

void AnimationControllerEditor::HandleCreateAndDelete()
{
    if (ed::BeginCreate())
    {
        ed::PinId inputPinID, outputPinID;
        if (ed::QueryNewLink(&inputPinID, &outputPinID))
        {
            if (inputPinID && outputPinID)
            {
                // BY KIND: the source end must be some node's OUTPUT and the target
                // end some node's INPUT. Releasing a drag over the Any State node
                // snaps to its only registered pin -- its output -- so without this
                // the pair resolves output-to-output, both ends find a real node, and
                // the check below inspects the wrong one. The link then draws on the
                // canvas and BuildGraph drops it on save: a transition that looks
                // real and is not. Failing to resolve is what rejects it.
                const ControllerNode* from = FindNodeByOutputPin(outputPinID);
                const ControllerNode* to   = FindNodeByInputPin(inputPinID);

                // Three rules, and only the first differs from a plain node graph:
                //  - nothing transitions INTO Any State (it is a source, not a place).
                //    Belt and braces now: Any State draws no input pin, so no link can
                //    legitimately name one;
                //  - a state may have MANY outgoing transitions, unlike an audio
                //    chain's one, which is the whole shape of a state machine;
                //  - a self-transition is rejected -- re-entering the current state
                //    is what CrossFade expresses explicitly, and as a graph edge it
                //    would fire forever.
                const bool valid = from && to
                                && to->kind != ControllerNodeKind::AnyState
                                && from->id != to->id;

                if (valid && ed::AcceptNewItem())
                {
                    ControllerLink link;
                    link.linkID   = ed::LinkId(NextID());
                    link.inputID  = inputPinID;
                    link.outputID = outputPinID;

                    m_links.push_back(std::move(link));
                    ed::Link(m_links.back().linkID, m_links.back().inputID, m_links.back().outputID);
                    m_dirty = true;
                }
                else if (!valid)
                {
                    ed::RejectNewItem(ImColor(220, 90, 90), 2.0f);
                }
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
                        m_dirty = true;
                        break;
                    }
                }
            }
        }

        ed::NodeId deletedNodeID;
        while (ed::QueryDeletedNode(&deletedNodeID))
        {
            ControllerNode* node = FindNodeById(deletedNodeID);

            // Any State is part of the graph's vocabulary, not something the user
            // added, so it is refused rather than silently recreated next frame.
            if (!node || node->kind == ControllerNodeKind::AnyState)
            {
                ed::RejectDeletedItem();
                continue;
            }

            if (ed::AcceptDeletedItem())
            {
                // A deleted state takes its clip's reference with it, and its links
                // with it: a link left behind would name a pin nothing owns.
                if (node->clip)
                {
                    if (ModuleResourceManager* rm = ResourceManager())
                        rm->UnloadResource(node->clip->GetUID());
                    node->clip = nullptr;
                }

                const ed::PinId in  = node->input.id;
                const ed::PinId out = node->output.id;

                std::erase_if(m_links, [in, out](const ControllerLink& l)
                {
                    return l.inputID == in || l.outputID == out;
                });

                if (m_defaultStateName == node->state.name)
                    m_defaultStateName.clear();

                for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it)
                {
                    if (it->id == deletedNodeID)
                    {
                        m_nodes.erase(it);
                        break;
                    }
                }

                m_dirty = true;
            }
        }
    }
    ed::EndDelete();
}

ControllerNode* AnimationControllerEditor::NodeAtScreenPos(const ImVec2& screenPos)
{
    // Canvas space, because that is what GetNodePosition reports -- comparing a
    // screen point against canvas rects gets steadily wronger as the user pans or
    // zooms, which is the kind of bug that only shows up away from the origin.
    ed::SetCurrentEditor(m_context);
    const ImVec2 canvasPos = ed::ScreenToCanvas(screenPos);

    ControllerNode* hit = nullptr;

    for (ControllerNode& n : m_nodes)
    {
        if (n.kind != ControllerNodeKind::State)
            continue;   // Any State has no clip to rebind

        const ImVec2 min  = ed::GetNodePosition(n.id);
        const ImVec2 size = ed::GetNodeSize(n.id);

        if (canvasPos.x >= min.x && canvasPos.x <= min.x + size.x &&
            canvasPos.y >= min.y && canvasPos.y <= min.y + size.y)
        {
            hit = &n;
            break;
        }
    }

    ed::SetCurrentEditor(nullptr);
    return hit;
}

void AnimationControllerEditor::HandleCanvasDrop(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    // BeginDragDropTargetCustom registers a drop zone over an arbitrary rect with no
    // owning item, and only goes live while an external payload is being dragged --
    // so it never steals the clicks the node editor needs for panning and selection.
    const ImRect rect(canvasMin, canvasMax);
    if (!ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID("AnimControllerCanvasDrop")))
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
    {
        // A .nctrl wins over a clip when both arrive in one multi-select drop:
        // "open this controller" is a whole-window action and cannot be half-done.
        if (const std::string nctrl = FirstDroppedAsset(payload, ".nctrl"); !nctrl.empty())
        {
            OpenAsset(nctrl);
        }
        else if (const std::string nanim = FirstDroppedAsset(payload, ".nanim"); !nanim.empty())
        {
            const ImVec2 dropPos = ImGui::GetMousePos();

            if (ControllerNode* node = NodeAtScreenPos(dropPos))
            {
                // REBIND, leaving every transition intact. This is what makes the
                // graph editable rather than only rebuildable -- swapping a
                // placeholder for the real animation must not cost the edges.
                SetNodeClip(*node, nanim);
            }
            else
            {
                ed::SetCurrentEditor(m_context);
                const ImVec2 canvasPos = ed::ScreenToCanvas(dropPos);
                ed::SetCurrentEditor(nullptr);

                m_nodes.push_back(MakeNode(ControllerNodeKind::State, canvasPos));
                ControllerNode& created = m_nodes.back();

                SetNodeClip(created, nanim);

                // Named after the clip, which is what the user just pointed at. Through
                // UniqueStateName so dropping the same clip twice cannot produce two
                // states that FindState is unable to tell apart.
                if (created.clip)
                    created.state.name = UniqueStateName(created.clip->GetName(), created.id);

                // The first state authored becomes the default, so a fresh controller
                // does not sit on a NoDefaultState warning until the user finds the
                // Set as Default button.
                if (m_defaultStateName.empty())
                    m_defaultStateName = created.state.name;

                m_dirty = true;
            }
        }
    }

    ImGui::EndDragDropTarget();
}

void AnimationControllerEditor::DrawCanvas()
{
    ed::SetCurrentEditor(m_context);
    ed::Begin("AnimationController", ImVec2(0.0f, 0.0f));
    {
        const ImVec2 screenMin = ImGui::GetWindowPos();
        const ImVec2 screenMax = ImVec2(screenMin.x + ImGui::GetWindowSize().x,
                                        screenMin.y + ImGui::GetWindowSize().y);
        const ImVec2 screenMid{ (screenMin.x + screenMax.x) * 0.5f,
                                (screenMin.y + screenMax.y) * 0.5f };
        m_spawnPosition = ed::ScreenToCanvas(screenMid);

        // Resolved ONCE per frame: the lookup walks the scene's selection, and one
        // answer for the whole frame is also what keeps the node tint and the link
        // tint from ever disagreeing.
        const CAnimator*       watched     = WatchedAnimator();
        const std::string_view activeState = watched ? watched->GetCurrentStateName() : std::string_view{};
        const bool             fading      = watched && watched->IsFading();

        for (ControllerNode& node : m_nodes)
            DrawNode(node, activeState);

        // The in-flight transition is highlighted BY TARGET, not by identity: the
        // animator does not expose which transition object fired, and carrying one
        // solely for a debug tint is not worth the member. The cost is that two
        // transitions into the same state both light up -- acceptable, because the
        // thing being answered is "where is it going", which both agree on.
        const ControllerNode* activeNode = nullptr;
        if (fading && !activeState.empty())
            for (const ControllerNode& n : m_nodes)
                if (n.kind == ControllerNodeKind::State && n.state.name == activeState)
                    activeNode = &n;

        for (const ControllerLink& link : m_links)
        {
            const bool incoming = activeNode && link.inputID == activeNode->input.id;

            if (incoming)
            {
                ed::Link(link.linkID, link.inputID, link.outputID, ImColor(255, 205, 90, 255), 3.0f);
            }
            else
            {
                ed::Link(link.linkID, link.inputID, link.outputID);
            }
        }

        // The library's built-in Delete handler only fires when its
        // CanAcceptUserInput() is true, which is flaky in this host setup. On the
        // Spanish ISO layout (via SDL) the physical "Supr" key reports as
        // SDL_SCANCODE_KP_PERIOD -> ImGuiKey_KeypadDecimal, and the upper-row variant
        // arrives as ImGuiKey_Pause -- so listen for those, not ImGuiKey_Delete.
        const bool deletePressed =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            (ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal, false) ||
             ImGui::IsKeyPressed(ImGuiKey_Pause,         false));

        if (deletePressed)
        {
            ed::NodeId selectedNodes[64];
            const int  selectedNodeCount = ed::GetSelectedNodes(selectedNodes, IM_ARRAYSIZE(selectedNodes));
            for (int i = 0; i < selectedNodeCount; ++i)
                ed::DeleteNode(selectedNodes[i]);

            ed::LinkId selectedLinks[128];
            const int  selectedLinkCount = ed::GetSelectedLinks(selectedLinks, IM_ARRAYSIZE(selectedLinks));
            for (int i = 0; i < selectedLinkCount; ++i)
                ed::DeleteLink(selectedLinks[i]);
        }

        HandleCreateAndDelete();
    }
    ed::End();

    // Deferred to frame 2: on frame 0 nodes are only SetNodePosition'd, and their
    // measured size is finalized one ImGui layout pass later. Fitting before node
    // bounds exist produces a wrong fit.
    if (!m_initialFitDone && m_nodes.size() > 1 && m_framesSinceOpen >= 1)
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

void AnimationControllerEditor::DrawContent()
{
    if (!m_context)
        return;

    DrawMenuBar();
    DrawNewAssetPopup();

    // The left panel is also load-bearing as layout: imgui-node-editor's canvas-rect
    // calculation needs a real MEASURED item in the host window before ed::Begin, and
    // an ImGui::Dummy does not satisfy it. A SIBLING child is fine -- what must never
    // happen is wrapping ed::Begin itself in a BeginChild.
    ImGui::BeginChild("##controllerLeft", ImVec2(280.0f, 0.0f), true);
    DrawLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Captured BEFORE ed::Begin advances the cursor, so the whole remaining window
    // area can be wired as a drop target once the canvas has drawn.
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 avail     = ImGui::GetContentRegionAvail();
    const ImVec2 canvasMax = ImVec2(canvasMin.x + avail.x, canvasMin.y + avail.y);

    DrawCanvas();

    HandleCanvasDrop(canvasMin, canvasMax);
}

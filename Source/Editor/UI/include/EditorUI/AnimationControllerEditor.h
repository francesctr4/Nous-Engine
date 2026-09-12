#pragma once

#include <EditorUI/IEditorWindow.h>
#include <EditorCore/EditorExport.h>

#include <EditorUI/AnimationControllerBuild.h>

#include <imgui-node-editor/imgui_node_editor.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class CAnimator;
class ResourceAnimation;
class ResourceAnimationController;
class ModuleResourceManager;

// Any State is a SOURCE of transitions, not a place the animator can be -- so it is
// a node kind rather than a state, and BuildGraph skips it when filling the state
// array. Giving it a state would shift every index after it.
enum class ControllerNodeKind
{
    AnyState,
    State
};

struct ControllerNodePin
{
    ax::NodeEditor::PinId   id;
    ax::NodeEditor::PinKind kind;
};

struct ControllerNode
{
    ax::NodeEditor::NodeId id;
    ControllerNodeKind     kind = ControllerNodeKind::State;
    ImVec2                 position{};
    bool                   positionPending = true;

    // Exactly one of each. A state has no reason for more: every transition out of it
    // is a link from the same output pin, and the graph -- not the pin -- is what
    // distinguishes them.
    ControllerNodePin input;
    ControllerNodePin output;

    // The authored state, mirroring ControllerState field for field, plus the clip
    // this window resolved for it.
    //
    // THE CLIP CARRIES A WINDOW-OWNED REFERENCE, one per node, acquired wherever the
    // pointer is set and released wherever it is replaced or dropped. Uniformly, and
    // not only for clips the user drags in: making the release conditional on where a
    // pointer came from is how the count starts climbing.
    nous::engine::animation_system::ControllerState state;
    ResourceAnimation*                             clip = nullptr;
};

struct ControllerLink
{
    ax::NodeEditor::LinkId linkID;
    ax::NodeEditor::PinId  inputID;
    ax::NodeEditor::PinId  outputID;

    // fromState / toState are OVERWRITTEN from the pin ids when the graph is built;
    // everything else here is authored in the selection panel.
    nous::engine::animation_system::ControllerTransition transition;
};

/**
 * @brief Visual authoring for .nctrl animation state machines.
 *
 * Structurally AudioGraphEditor: same base class, same imgui-node-editor host, same
 * asset New/Open/Save flow, same four host requirements (see the
 * imgui-node-editor-host-setup notes). What differs is the graph shape -- a state
 * machine rather than a linear chain -- so a node may have MANY outgoing links,
 * order among them is meaningful, and the flattening lives in the separately tested
 * AnimationControllerBuild.h rather than in this file.
 */
class AnimationControllerEditor : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit AnimationControllerEditor(const char* title, EditorContext* context,
                                                       bool start_open = true);
    NOUS_EDITOR_API ~AnimationControllerEditor() override;

protected:

    NOUS_EDITOR_API void Init() override;
    NOUS_EDITOR_API void DrawContent() override;
    NOUS_EDITOR_API bool Begin(bool& outVisible) override;
    NOUS_EDITOR_API void End() override;
    NOUS_EDITOR_API ImGuiWindowFlags GetWindowFlags() const override
    {
        return ImGuiWindowFlags_NoScrollbar
             | ImGuiWindowFlags_NoScrollWithMouse
             | ImGuiWindowFlags_MenuBar;
    }

private:

    void DrawMenuBar();
    void DrawNewAssetPopup();      // modal name prompt for File > New

    // The left column, and also the measured ImGui item the canvas needs before
    // ed::Begin -- imgui-node-editor sizes its canvas from the host window's layout,
    // and an ImGui::Dummy is not enough.
    void DrawLeftPanel();
    void DrawParametersSection();
    void DrawSelectionSection();
    void DrawStateDetails(ControllerNode& node);
    void DrawTransitionDetails(ControllerLink& link);
    void DrawWarningsSection();

    void DrawCanvas();

    // `activeState` is the watched animator's current state name, or empty when
    // nothing is being watched. Passed in rather than resolved per node: the lookup
    // walks the scene's selection, and doing it once per frame instead of once per
    // node also guarantees every node is tinted against the same answer.
    void DrawNode(ControllerNode& node, std::string_view activeState);
    void HandleCreateAndDelete();

    // Full-canvas drop target. Three cases, and the second is the one that makes the
    // graph EDITABLE rather than only rebuildable: dropping a clip onto an existing
    // state rebinds it and leaves every transition intact, which is how a placeholder
    // gets swapped for the real animation.
    void HandleCanvasDrop(const ImVec2& canvasMin, const ImVec2& canvasMax);

    // The state node under a SCREEN-space point, or null. Used only by the drop
    // handler, to tell "rebind this state" from "create a new one".
    ControllerNode* NodeAtScreenPos(const ImVec2& screenPos);

    ControllerNode  MakeNode(ControllerNodeKind kind, ImVec2 position);
    void            SpawnState();

    // Resolved BY PIN KIND, deliberately as two functions rather than one that
    // matches either. A kind-agnostic lookup makes an output-to-output drag resolve
    // to two real nodes and pass validation, producing a link the canvas draws and
    // BuildGraph then drops -- a transition that appears to exist and does not.
    ControllerNode* FindNodeByOutputPin(ax::NodeEditor::PinId pin);
    ControllerNode* FindNodeByInputPin(ax::NodeEditor::PinId pin);

    ControllerNode* FindNodeById(ax::NodeEditor::NodeId id);
    const ControllerNode* AnyStateNode() const;

    // A name no other state uses and not the reserved "AnyState" literal. Returns a
    // suffixed variant when the desired name is taken, so a rename can never produce
    // two states that FindState cannot tell apart.
    std::string UniqueStateName(const std::string& desired, ax::NodeEditor::NodeId exclude) const;

    // Renames a declared parameter AND every condition naming it. Leaving conditions
    // stale would be reported correctly by the UndeclaredParameter warning, which is
    // exactly the avoidable kind of correct.
    void RenameParameter(size_t index, const std::string& newName);

    // Acquire-then-release, the same order the importer's clip slots follow. Passing
    // an empty path clears the slot.
    void SetNodeClip(ControllerNode& node, const std::string& nanimPath);
    void ReleaseNodeClips();

    ModuleResourceManager* ResourceManager() const;

    // The animator this window is watching: the selected GameObject's CAnimator, but
    // only while the scene is simulating AND its controller is the asset open here.
    // Null otherwise, and the highlight simply does not draw.
    //
    // ModuleScene::selectedGameObjects is public and already read directly by
    // InspectorWindow, HierarchyWindow and SceneViewport, so this needs no new
    // plumbing and no new interface -- the editor is the layer allowed to know both.
    const CAnimator* WatchedAnimator() const;

    // Dependency-free proxies for the pure builder. State clipIndex is filled here so
    // Validate can see which states have no clip.
    void BuildProxies(std::vector<nous::anim_editor::BuildNode>& outNodes,
                      std::vector<nous::anim_editor::BuildLink>& outLinks) const;

    void LoadFromResource(ResourceAnimationController* controller);
    bool SaveToOpenAsset();
    void NewAsset(const std::string& name);
    void OpenAsset(const std::string& nctrlPath);

    std::uintptr_t NextID() { return m_nextID++; }

    ax::NodeEditor::EditorContext* m_context = nullptr;

    // Currently-open asset (ref held through the resource manager). null = scratch.
    ResourceAnimationController* m_controller = nullptr;

    std::vector<ControllerNode> m_nodes;
    std::vector<ControllerLink> m_links;

    // Neither belongs to a node, so neither is re-derived from the canvas: the
    // parameter list is graph-wide, and the default is a property OF the graph that
    // happens to name a state.
    //
    // The default is held BY NAME rather than by index because the canvas reorders
    // and deletes nodes freely -- an index would silently retarget.
    std::vector<nous::engine::animation_system::ParameterDecl> m_parameters;
    std::string                                                m_defaultStateName;

    std::uintptr_t m_nextID          = 1;
    int            m_framesSinceOpen = 0;
    bool           m_initialFitDone  = false;
    bool           m_dirty           = false;

    // File > New name prompt: a deferred flag opens the modal at window scope
    // (OpenPopup inside the menu bar mis-scopes on the ID stack).
    bool m_showNewAssetPopup = false;
    char m_newAssetName[128] = {};

    ImVec2 m_spawnPosition{ 40.0f, 40.0f };
    int    m_spawnStrideCount = 0;
};

#pragma once

#include <EditorUI/IEditorWindow.h>
#include <EditorCore/EditorExport.h>

#include <AudioSystem/AudioGraph/AudioEffectRegistry.h>
#include <AudioSystem/SoundHandle.h>
#include <AudioSystem/EffectChainHandle.h>
#include <EditorUI/AudioGraphLinearize.h>

#include <imgui-node-editor/imgui_node_editor.h>

#include <string>
#include <vector>
#include <cstdint>

class ResourceAudioGraph;
class ResourceAudio;
class ModuleResourceManager;
class ModuleAudio;

// What a node IS structurally: one of the two fixed anchors, or an effect.
//
// This deliberately does NOT enumerate the effects. The predecessor enum
// (AudioNodeKind) shadowed AudioEffectType value-for-value and forced four
// lockstep switches in this window, so every new effect cost an editor edit.
// An effect node now carries its AudioEffectType directly and reads everything
// else — label, category, parameter schema — from AudioEffectRegistry.
enum class AudioNodeRole : uint8_t
{
    Source,   // input anchor (the voice)
    Output,   // output anchor (the bus)
    Effect
};

// Presentation grouping. The two anchor values are editor-only; the other two
// mirror AudioEffectCategory, which the registry owns.
enum class AudioNodeCategory : uint8_t
{
    Source,
    Output,
    Effect,
    Utility
};

struct AudioNodePin
{
    ax::NodeEditor::PinId   id;
    ax::NodeEditor::PinKind kind;
    std::string             name;
};

struct AudioNode
{
    ax::NodeEditor::NodeId    id;
    AudioNodeRole             role = AudioNodeRole::Effect;
    AudioEffectType           effect{};   // meaningful only when role == Effect
    std::string               name;
    ImVec2                    position{};
    bool                      positionPending = true;
    std::vector<AudioNodePin> inputs;
    std::vector<AudioNodePin> outputs;
    std::vector<float>        params;   // effect params (parallel to the effect's schema); empty for anchors

    [[nodiscard]] bool IsEffect() const { return role == AudioNodeRole::Effect; }
};

struct AudioGraphLink
{
    ax::NodeEditor::LinkId linkID;
    ax::NodeEditor::PinId  inputID;
    ax::NodeEditor::PinId  outputID;
};

class AudioGraphEditor : public IEditorWindow
{
public:

    NOUS_EDITOR_API explicit AudioGraphEditor(const char* title, EditorContext* context, bool start_open = true);
    NOUS_EDITOR_API ~AudioGraphEditor() override;

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
    void DrawNewAssetPopup();  // modal name prompt for File ▸ New
    void DrawToolbar();        // preview controls + help; also the measured item before ed::Begin
    void DrawCanvas();
    void DrawNode(AudioNode& node);
    void HandleCreateAndDelete();
    // Full-canvas drop target: .nafx → open graph, .wav/.ogg → preview clip.
    void HandleCanvasDrop(const ImVec2& canvasMin, const ImVec2& canvasMax);

    // `effect` is ignored unless role == Effect.
    AudioNode MakeNode(AudioNodeRole role, AudioEffectType effect, ImVec2 position);
    void      SpawnNode(AudioNodeRole role, AudioEffectType effect);

    static AudioNodeCategory GetCategory(const AudioNode& node);
    static const char*       GetNodeLabel(AudioNodeRole role, AudioEffectType effect);
    static ImU32             GetCategoryHeaderColor(AudioNodeCategory category);
    static ImColor           GetCategoryBorderColor(AudioNodeCategory category);

    // Called when an inline param widget changes — pushes the value live to an
    // active preview chain by re-splicing it from the current graph.
    void OnParamEdited(const AudioNode& node, int paramIndex);

    // --- In-editor preview (Task 5) ---
    ModuleAudio* AudioModule() const;     // editorContext->GetScene()->GetAudio()
    void PreviewPlay();                   // (re)create the preview voice + chain and start it
    void PreviewStop();                   // tear down chain (before voice) then voice
    void RebuildPreviewChain();           // re-splice the chain from the live graph (topology change)
    void SetPreviewClip(const std::string& audioPath);   // resolve + swap the audition clip (ref-managed)
    // Position of `node` among the effect nodes in the live chain order, or -1 if it
    // is not on the Source→Output path. Used to target a live SetEffectParam patch.
    int EffectIndexInChain(const AudioNode& node);

    // --- Asset binding (Task 4) ---
    ModuleResourceManager* ResourceManager() const;
    // Build dependency-free proxies of the current graph for the pure linearizer.
    void BuildProxies(std::vector<nous::audio_editor::LinNode>& outNodes,
                      std::vector<nous::audio_editor::LinLink>& outLinks) const;
    bool                    LinearizeCurrent(AudioGraphDesc& outDesc) const;  // editor → desc
    std::vector<AudioNode*> ChainOrderedNodes();                              // [Source, fx..., Output] or empty
    void                    LoadFromResource(ResourceAudioGraph* graph);      // rebuild canvas from an asset
    bool                    SaveToOpenAsset();                                // write desc+positions, bump generation
    void                    NewAsset(const std::string& name);                // create + open a fresh <name>.nafx
    void                    OpenAsset(const std::string& nafxPath);           // resolve + LoadFromResource

    std::uintptr_t NextID() { return m_nextID++; }

    ax::NodeEditor::EditorContext* m_context = nullptr;

    // Currently-open asset (ref held via the resource manager). null = scratch graph.
    ResourceAudioGraph* m_graph = nullptr;

    // Preview audition: a user-picked clip auditioned through the live chain.
    // The chain hangs off the voice, so it must be destroyed before the voice
    // (PreviewStop enforces this). All null when no preview is active.
    ResourceAudio*    m_previewClip  = nullptr;
    SoundHandle       m_previewVoice = nullptr;
    EffectChainHandle m_previewChain = nullptr;

    std::vector<AudioNode>      m_nodes;
    std::vector<AudioGraphLink> m_links;

    std::uintptr_t m_nextID          = 1;
    int            m_framesSinceOpen = 0;
    bool           m_initialFitDone  = false;
    bool           m_dirty           = false;  // unsaved edits (param tweak / topology change)

    // File ▸ New name prompt: a deferred flag opens the modal at window scope
    // (OpenPopup inside the menu bar mis-scopes on the ID stack).
    bool m_showNewAssetPopup = false;
    char m_newAssetName[128] = {};

    // Canvas-local position where the next spawned node will be placed.
    // Refreshed each frame from the canvas view center; menu spawns offset
    // by an increasing stride so multiple nodes don't stack on top.
    ImVec2 m_spawnPosition{ 40.0f, 40.0f };
    int    m_spawnStrideCount = 0;
};

#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Editor/EditorExport.h"

#include "Engine/Systems/AudioSystem/AudioGraph/AudioEffectRegistry.h"
#include "Editor/UI/Windows/AudioGraphEditor/AudioGraphLinearize.h"

#include <imgui-node-editor/imgui_node_editor.h>

#include <string>
#include <vector>
#include <cstdint>

class ResourceAudioGraph;
class ModuleResourceManager;

// MVP palette: two fixed anchors (the voice in, the bus out) plus one node per
// stock effect. Effect kinds map 1:1 to AudioEffectType (see ToEffectType).
enum class AudioNodeKind
{
    AudioSource,   // input anchor (the voice)
    AudioOutput,   // output anchor (the bus)
    LowPass,
    HighPass,
    Delay,
    Gain
};

enum class AudioNodeCategory
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
    AudioNodeKind             kind;
    std::string               name;
    ImVec2                    position{};
    bool                      positionPending = true;
    std::vector<AudioNodePin> inputs;
    std::vector<AudioNodePin> outputs;
    std::vector<float>        params;   // effect params (parallel to the kind's schema); empty for anchors
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
    void DrawCanvas();
    void DrawNode(AudioNode& node);
    void HandleCreateAndDelete();

    AudioNode MakeNode(AudioNodeKind kind, ImVec2 position);
    void      SpawnNode(AudioNodeKind kind);

    static AudioNodeCategory GetCategory(AudioNodeKind kind);
    static const char*       GetKindLabel(AudioNodeKind kind);
    static ImU32             GetCategoryHeaderColor(AudioNodeCategory category);
    static ImColor           GetCategoryBorderColor(AudioNodeCategory category);

    static bool            IsEffect(AudioNodeKind kind);
    static AudioEffectType ToEffectType(AudioNodeKind kind);   // valid only when IsEffect(kind)
    static AudioNodeKind   FromEffectType(AudioEffectType type);

    // Called when an inline param widget changes — pushes the value live to an
    // active preview chain (filled in Task 5; no-op until then).
    void OnParamEdited(const AudioNode& node, int paramIndex);

    std::uintptr_t NextID() { return m_nextID++; }

    ax::NodeEditor::EditorContext* m_context = nullptr;

    std::vector<AudioNode>      m_nodes;
    std::vector<AudioGraphLink> m_links;

    std::uintptr_t m_nextID          = 1;
    int            m_framesSinceOpen = 0;
    bool           m_initialFitDone  = false;
    bool           m_dirty           = false;  // unsaved edits (param tweak / topology change)

    // Canvas-local position where the next spawned node will be placed.
    // Refreshed each frame from the canvas view center; menu spawns offset
    // by an increasing stride so multiple nodes don't stack on top.
    ImVec2 m_spawnPosition{ 40.0f, 40.0f };
    int    m_spawnStrideCount = 0;
};

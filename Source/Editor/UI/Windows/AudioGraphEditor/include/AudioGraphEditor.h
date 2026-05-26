#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Editor/EditorExport.h"

#include <imgui-node-editor/imgui_node_editor.h>

#include <string>
#include <vector>
#include <cstdint>

enum class AudioNodeKind
{
    AudioSource,
    AudioOutput,
    Equalizer,
    LowPassFilter,
    HighPassFilter,
    Reverb,
    Delay,
    Gain,
    Mixer,
    Splitter
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

    std::uintptr_t NextID() { return m_nextID++; }

    ax::NodeEditor::EditorContext* m_context = nullptr;

    std::vector<AudioNode>      m_nodes;
    std::vector<AudioGraphLink> m_links;

    std::uintptr_t m_nextID     = 1;
    bool           m_firstFrame = true;

    // Canvas-local position where the next spawned node will be placed.
    // Refreshed each frame from the canvas view center; menu spawns offset
    // by an increasing stride so multiple nodes don't stack on top.
    ImVec2 m_spawnPosition{ 40.0f, 40.0f };
    int    m_spawnStrideCount = 0;
};

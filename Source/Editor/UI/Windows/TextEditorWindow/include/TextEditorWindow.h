#pragma once

#include "Editor/UI/IEditorWindow.inl"
#include "Editor/../../ThirdParty/ImGuiColorTextEdit/TextEditor.h"

#include <atomic>
#include <filesystem>
#include <string>

enum class TextEditorMode : uint8_t { Shader, Script };

class TextEditorWindow : public IEditorWindow
{
public:
    explicit TextEditorWindow(const char* title, EditorContext* context, bool start_open = true);
    ~TextEditorWindow() override = default;

    void Init() override;
    void Draw() override;

    // Load a file into the editor (callable from AssetsBrowser or other windows).
    // Switches mode automatically based on file extension.
    void LoadFile(const std::filesystem::path& filePath);

private:
    void DrawTabs();
    void DrawToolbar();
    void CreateNew();
    void OpenFile();
    void Save();
    void Delete();

    void        SwitchMode(TextEditorMode mode);
    void        TrySwitch(TextEditorMode target);  // guards unsaved changes before SwitchMode
    std::string GetScriptTemplate() const;
    void        TriggerScriptRecompile();

    static constexpr const char* k_AssetsShaderPath   = "Assets/Shaders/";
    static constexpr const char* k_AssetsScriptPath   = "Assets/Scripts/";
    static constexpr const char* k_GlslExtension      = ".glsl";
    static constexpr const char* k_CppExtension       = ".cpp";
    static constexpr const char* k_ScriptTemplatePath = "Library/Scripts/ScriptTemplate.inl";
    static constexpr const char* k_ClassNameToken     = "$CLASSNAME$";

    // Minimal GLSL template matching the engine's unified shader format.
    static constexpr const char* k_DefaultShaderSource =
        "#pragma stage vertex\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) in vec3 inPosition;\n"
        "layout(location = 1) in vec3 inNormal;\n"
        "layout(location = 2) in vec3 inColor;\n"
        "layout(location = 3) in vec2 inTexCoord;\n"
        "\n"
        "layout(set = 0, binding = 0) uniform globalUniformObject\n"
        "{\n"
        "    mat4 projection;\n"
        "    mat4 view;\n"
        "} globalUBO;\n"
        "\n"
        "layout(push_constant) uniform pushConstantObject\n"
        "{\n"
        "    mat4 model;\n"
        "} pushConstant;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = globalUBO.projection * globalUBO.view * pushConstant.model * vec4(inPosition, 1.0);\n"
        "}\n"
        "\n"
        "// -------------------------------------------------------------------\n"
        "\n"
        "#pragma stage fragment\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "}\n";

private:
    TextEditor            mTextEditor;
    TextEditorMode        mMode              = TextEditorMode::Shader;
    std::filesystem::path mCurrentFilePath;
    char                  mFileNameBuffer[256];
    bool                  mHasUnsavedChanges = false;

    // Pending mode switch state (used while "Discard changes?" popup is open)
    bool           mPendingModeSwitch = false;
    TextEditorMode mPendingMode       = TextEditorMode::Shader;

    // Guards against overlapping script recompile jobs (mirrors shader system's m_inFlightPaths).
    // Set to true when a job is submitted; cleared by the job lambda when it finishes.
    std::atomic<bool> mScriptRecompileInFlight { false };
};

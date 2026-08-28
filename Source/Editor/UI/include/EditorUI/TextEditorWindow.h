#pragma once

#include <EditorUI/IEditorWindow.h>
#include <TextEditor.h>

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
    void Update() override;
    void DrawContent() override;

    // Load a file into the editor (callable from AssetsBrowser or other windows).
    // Switches mode automatically based on file extension.
    void LoadFile(const std::filesystem::path& filePath);

    // GLSL template matching the engine's unified shader format (no diffuse sampler).
    // Public so AssetsBrowser can write it when creating a new shader asset.
    static constexpr const char* k_DefaultShaderSource =
        "#pragma stage vertex\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) in vec3 inPosition;\n"
        "layout(location = 1) in vec3 inNormal;\n"
        "layout(location = 2) in vec3 inColor;\n"
        "layout(location = 3) in vec2 inTexCoord;\n"
        "\n"
        "struct DirectionalLight { vec4 direction; vec4 color; };\n"
        "struct PointLight        { vec4 position; vec4 color; };\n"
        "struct SpotLight         { vec4 position; vec4 direction; vec4 color; vec4 angles; };\n"
        "\n"
        "layout(set = 0, binding = 0) uniform GlobalUBO\n"
        "{\n"
        "    mat4             projection;\n"
        "    mat4             view;\n"
        "    vec4             viewPosition;\n"
        "    vec4             ambientColor;\n"
        "    DirectionalLight dirLight;\n"
        "    ivec4            lightCountAndPad;\n"
        "    PointLight       pointLights[16];\n"
        "    vec4             time;  // x=totalTime, y=sin(t), z=cos(t), w=deltaTime\n"
        "    ivec4            spotLightCountAndPad;\n"
        "    SpotLight        spotLights[8];\n"
        "} globalUBO;\n"
        "\n"
        "layout(set = 0, binding = 1) readonly buffer InstanceData\n"
        "{\n"
        "    mat4 models[];\n"
        "} instanceData;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = globalUBO.projection * globalUBO.view * instanceData.models[gl_InstanceIndex] * vec4(inPosition, 1.0);\n"
        "}\n"
        "\n"
        "// -------------------------------------------------------------------\n"
        "\n"
        "#pragma stage fragment\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "\n"
        "struct DirectionalLight { vec4 direction; vec4 color; };\n"
        "struct PointLight        { vec4 position; vec4 color; };\n"
        "struct SpotLight         { vec4 position; vec4 direction; vec4 color; vec4 angles; };\n"
        "\n"
        "layout(set = 0, binding = 0) uniform GlobalUBO\n"
        "{\n"
        "    mat4             projection;\n"
        "    mat4             view;\n"
        "    vec4             viewPosition;\n"
        "    vec4             ambientColor;\n"
        "    DirectionalLight dirLight;\n"
        "    ivec4            lightCountAndPad;\n"
        "    PointLight       pointLights[16];\n"
        "    vec4             time;  // x=totalTime, y=sin(t), z=cos(t), w=deltaTime\n"
        "    ivec4            spotLightCountAndPad;\n"
        "    SpotLight        spotLights[8];\n"
        "} globalUBO;\n"
        "\n"
        "layout(set = 1, binding = 0) uniform InstanceUBO\n"
        "{\n"
        "    vec4 diffuseColor;  // rgb = tint, a = opacity (unused)\n"
        "} instanceUBO;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    fragColor = instanceUBO.diffuseColor;\n"
        "}\n";

private:
    void DrawTabs();
    void DrawToolbar();
    void HandleDragDropTarget();
    void CreateNew();
    void OpenFile();
    void Save();
    void Delete();

    void        SwitchMode(TextEditorMode mode);
    void        TrySwitch(TextEditorMode target);  // guards unsaved changes before SwitchMode
    std::string GetScriptTemplate() const;
    void        TriggerScriptRecompile();
    void        TriggerShaderImport(const std::filesystem::path& filePath);

    static constexpr const char* k_AssetsShaderPath   = "Assets/Shaders/";
    static constexpr const char* k_AssetsScriptPath   = "Assets/Scripts/";
    static constexpr const char* k_GlslExtension      = ".glsl";
    static constexpr const char* k_CppExtension       = ".cpp";
    static constexpr const char* k_HeaderExtension    = ".h";
    static constexpr const char* k_ScriptTemplatePath = "EngineSDK/Scripts/ScriptTemplate.inl";
    static constexpr const char* k_ClassNameToken     = "$CLASSNAME$";

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

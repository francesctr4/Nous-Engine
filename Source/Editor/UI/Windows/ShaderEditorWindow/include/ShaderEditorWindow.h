#ifndef SHADEREDITRWINDOW_H
#define SHADEREDITRWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include "Editor/../../ThirdParty/ImGuiColorTextEdit/TextEditor.h"

#include <filesystem>
#include <string>

class ShaderEditorWindow : public IEditorWindow
{
public:
    explicit ShaderEditorWindow(const char* title, EditorContext* context, bool start_open = true);
    ~ShaderEditorWindow() override = default;

    void Init() override;
    void Draw() override;

    // Load a .glsl shader file into the editor (callable from AssetsBrowser or other windows).
    void LoadShader(const std::filesystem::path& filePath);

private:
    void DrawToolbar();
    void CreateNewShader();
    void OpenShader();
    void SaveShader();
    void DeleteShader();

    static constexpr const char* k_AssetsShaderPath = "Assets/Shaders/";
    static constexpr const char* k_GlslExtension    = ".glsl";

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
    std::filesystem::path mCurrentFilePath;
    char                  mFileNameBuffer[256];
    bool                  mHasUnsavedChanges = false;
};

#endif // SHADEREDITRWINDOW_H

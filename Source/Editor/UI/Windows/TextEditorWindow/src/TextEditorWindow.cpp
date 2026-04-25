#include "Editor/UI/Windows/TextEditorWindow/include/TextEditorWindow.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include "imgui.h"

#include <fstream>
#include <sstream>

#ifdef _WIN32
#  include <Windows.h>
#endif

static std::filesystem::path GetExeDir()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

TextEditorWindow::TextEditorWindow(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    mFileNameBuffer[0] = '\0';
}

void TextEditorWindow::Init()
{
    mTextEditor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
}

void TextEditorWindow::Update()
{
    ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);
}

void TextEditorWindow::DrawContent()
{
    DrawTabs();

    ImGui::Separator();
    ImGui::Spacing();

    DrawToolbar();

    ImGui::Separator();
    ImGui::Spacing();

    mTextEditor.Render("##TextEditorContent", ImGui::GetContentRegionAvail());

    // ImGuiColorTextEdit doesn't set io.WantTextInput like standard ImGui
    // widgets do. Set it manually when the TextEditor's child window is focused
    // so that ImGui_ImplSDL3_NewFrame() keeps SDL_StartTextInput() active.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
        ImGui::GetIO().WantTextInput = true;

    if (mTextEditor.IsTextChanged())
        mHasUnsavedChanges = true;

    HandleDragDropTarget();
}

void TextEditorWindow::HandleDragDropTarget()
{
    // Accept .glsl and .cpp files dragged from the AssetsBrowser.
    // Drag-drop bypasses the unsaved-changes guard — it is an explicit user action.
    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
    {
        const auto* data = static_cast<const char*>(payload->Data);
        const char* end = data + payload->DataSize;
        while (data < end)
        {
            std::string path(data);
            data += path.size() + 1;

            std::filesystem::path p(path);
            const auto ext = p.extension();

            if (ext == k_GlslExtension)
            {
                SwitchMode(TextEditorMode::Shader);
                LoadFile(p);
            }
            else if (ext == k_CppExtension)
            {
                SwitchMode(TextEditorMode::Script);
                LoadFile(p);
            }
        }
    }

    ImGui::EndDragDropTarget();
}

void TextEditorWindow::DrawTabs()
{
    if (!ImGui::BeginTabBar("##TextEditorTabs"))
        return;

    // While the discard popup is pending, force the current mode's tab to stay selected.
    const ImGuiTabItemFlags shaderFlags = (mPendingModeSwitch && mMode == TextEditorMode::Shader)
                                              ? ImGuiTabItemFlags_SetSelected
                                              : ImGuiTabItemFlags_None;
    const ImGuiTabItemFlags scriptFlags = (mPendingModeSwitch && mMode == TextEditorMode::Script)
                                              ? ImGuiTabItemFlags_SetSelected
                                              : ImGuiTabItemFlags_None;

    if (ImGui::BeginTabItem("Shaders", nullptr, shaderFlags))
    {
        if (mMode != TextEditorMode::Shader && !mPendingModeSwitch)
            TrySwitch(TextEditorMode::Shader);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Scripts", nullptr, scriptFlags))
    {
        if (mMode != TextEditorMode::Script && !mPendingModeSwitch)
            TrySwitch(TextEditorMode::Script);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    // Discard changes confirmation popup
    if (ImGui::BeginPopupModal("##DiscardChanges", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Unsaved changes will be lost. Continue?");
        ImGui::Spacing();

        if (ImGui::Button("Discard"))
        {
            SwitchMode(mPendingMode);
            mPendingModeSwitch = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            mPendingModeSwitch = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void TextEditorWindow::DrawToolbar()
{
    ImGui::Spacing();

    // ---- File name input ----
    ImGui::Text("File:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##FileName", mFileNameBuffer, sizeof(mFileNameBuffer));

    ImGui::SameLine();
    if (ImGui::Button("New"))
        CreateNew();

    ImGui::SameLine();
    if (ImGui::Button("Open"))
        OpenFile();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
    if (ImGui::Button("Save"))
        Save();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("Delete"))
        Delete();
    ImGui::PopStyleColor();

    // ---- Status indicators ----
    if (mHasUnsavedChanges)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "(unsaved)");
    }

    if (!mCurrentFilePath.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", mCurrentFilePath.string().c_str());
    }

    // ---- Cursor position info ----
    auto cursor = mTextEditor.GetCursorPosition();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120.0f);
    ImGui::TextDisabled("Ln %d  Col %d", cursor.mLine + 1, cursor.mColumn + 1);

    ImGui::Spacing();
}

void TextEditorWindow::CreateNew()
{
    if (mMode == TextEditorMode::Shader)
    {
        strncpy(mFileNameBuffer, "NewShader", sizeof(mFileNameBuffer) - 1);
        mFileNameBuffer[sizeof(mFileNameBuffer) - 1] = '\0';
        mTextEditor.SetText(k_DefaultShaderSource);
    }
    else
    {
        if (mFileNameBuffer[0] == '\0')
        {
            strncpy(mFileNameBuffer, "NewScript", sizeof(mFileNameBuffer) - 1);
            mFileNameBuffer[sizeof(mFileNameBuffer) - 1] = '\0';
        }
        mTextEditor.SetText(GetScriptTemplate());
    }

    mCurrentFilePath.clear();
    mHasUnsavedChanges = true;
}

void TextEditorWindow::OpenFile()
{
    if (mFileNameBuffer[0] == '\0')
    {
        NOUS_WARN("[TextEditor] Cannot open: file name is empty.");
        return;
    }

    const char* basePath = (mMode == TextEditorMode::Shader) ? k_AssetsShaderPath : k_AssetsScriptPath;
    const char* ext = (mMode == TextEditorMode::Shader) ? k_GlslExtension : k_CppExtension;

    std::filesystem::path loadPath = std::filesystem::path(basePath)
        / (std::string(mFileNameBuffer) + ext);

    LoadFile(loadPath);
}

void TextEditorWindow::Save()
{
    if (mFileNameBuffer[0] == '\0')
    {
        NOUS_WARN("[TextEditor] Cannot save: file name is empty.");
        return;
    }

    const char* ext = (mMode == TextEditorMode::Shader) ? k_GlslExtension : k_CppExtension;

    std::filesystem::path savePath = std::filesystem::path(editorContext->GetAssetsBrowserDirectory())
        / (std::string(mFileNameBuffer) + ext);

    std::ofstream file(savePath);
    if (!file.is_open())
    {
        NOUS_ERROR("[TextEditor] Failed to open file for writing: %s", savePath.string().c_str());
        return;
    }

    file << mTextEditor.GetText();
    file.close();

    if (!file)
    {
        NOUS_ERROR("[TextEditor] Failed to write to file: %s", savePath.string().c_str());
        return;
    }

    mCurrentFilePath = savePath;
    mHasUnsavedChanges = false;
    NOUS_INFO("[TextEditor] Saved: %s", savePath.string().c_str());

    if (mMode == TextEditorMode::Script)
        TriggerScriptRecompile();
}

void TextEditorWindow::Delete()
{
    if (mCurrentFilePath.empty())
    {
        NOUS_WARN("[TextEditor] No file loaded to delete.");
        return;
    }

    if (std::error_code ec; std::filesystem::remove(mCurrentFilePath, ec))
    {
        NOUS_INFO("[TextEditor] Deleted: %s", mCurrentFilePath.string().c_str());
    }
    else
    {
        NOUS_ERROR("[TextEditor] Failed to delete: %s (%s)",
                   mCurrentFilePath.string().c_str(), ec.message().c_str());
    }

    mCurrentFilePath.clear();
    mFileNameBuffer[0] = '\0';
    mTextEditor.SetText("");
    mHasUnsavedChanges = false;
}

void TextEditorWindow::LoadFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        NOUS_ERROR("[TextEditor] Failed to open file: %s", filePath.string().c_str());
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    mCurrentFilePath = filePath;

    std::string stem = filePath.stem().string();
    strncpy(mFileNameBuffer, stem.c_str(), sizeof(mFileNameBuffer) - 1);
    mFileNameBuffer[sizeof(mFileNameBuffer) - 1] = '\0';

    mTextEditor.SetText(ss.str());
    mHasUnsavedChanges = false;

    NOUS_INFO("[TextEditor] Loaded: %s", filePath.string().c_str());
}

void TextEditorWindow::SwitchMode(TextEditorMode mode)
{
    mMode = mode;
    mCurrentFilePath.clear();
    mFileNameBuffer[0] = '\0';
    mTextEditor.SetText("");
    mHasUnsavedChanges = false;

    if (mode == TextEditorMode::Shader)
        mTextEditor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    else
        mTextEditor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
}

void TextEditorWindow::TrySwitch(TextEditorMode target)
{
    if (mHasUnsavedChanges)
    {
        mPendingModeSwitch = true;
        mPendingMode = target;
        ImGui::OpenPopup("##DiscardChanges");
    }
    else
    {
        SwitchMode(target);
    }
}

std::string TextEditorWindow::GetScriptTemplate() const
{
    const std::string className = (mFileNameBuffer[0] != '\0') ? mFileNameBuffer : "NewScript";

    const std::filesystem::path templatePath = GetExeDir() / k_ScriptTemplatePath;
    std::ifstream file(templatePath);
    if (!file.is_open())
    {
        NOUS_WARN("[TextEditor] Could not open script template: %s", templatePath.string().c_str());
        return "// Script template not found.\n";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    const std::string token = k_ClassNameToken;
    size_t pos = 0;
    while ((pos = content.find(token, pos)) != std::string::npos)
    {
        content.replace(pos, token.size(), className);
        pos += className.size();
    }

    return content;
}

void TextEditorWindow::TriggerScriptRecompile()
{
    if (mScriptRecompileInFlight.load())
    {
        NOUS_WARN("[TextEditor] Script recompile already in flight, skipping.");
        return;
    }

    mScriptRecompileInFlight.store(true);

    const ModuleScene* scene = editorContext->GetScene();
    nous::engine::multithreading::NOUS_JobSystem* jobSystem = editorContext->GetJobSystem();
    jobSystem->SubmitJob([scene, this]
    {
        scene->RecompileScripts();
        mScriptRecompileInFlight.store(false);
    }, "Script Hot-Reload");

    NOUS_INFO("[TextEditor] Script recompile triggered.");
}

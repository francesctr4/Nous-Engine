#include "Editor/UI/Windows/ShaderEditorWindow/include/ShaderEditorWindow.h"

#include "Engine/Core/Logger/Logger.h"

#include "imgui.h"

#include <fstream>
#include <sstream>

ShaderEditorWindow::ShaderEditorWindow(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    mFileNameBuffer[0] = '\0';
    Init();
}

void ShaderEditorWindow::Init()
{
    mTextEditor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
}

void ShaderEditorWindow::Draw()
{
    if (!*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title, p_open))
    {
        DrawToolbar();

        ImGui::Separator();
        ImGui::Spacing();

        mTextEditor.Render("##ShaderEditorText", ImGui::GetContentRegionAvail());

        // ImGuiColorTextEdit doesn't set io.WantTextInput like standard ImGui
        // widgets do. Set it manually when the TextEditor's child window is focused
        // so that ImGui_ImplSDL3_NewFrame() keeps SDL_StartTextInput() active.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            ImGui::GetIO().WantTextInput = true;

        if (mTextEditor.IsTextChanged())
            mHasUnsavedChanges = true;

        // Accept .glsl files dragged from the AssetsBrowser.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
            {
                const char* data = static_cast<const char*>(payload->Data);
                const char* end  = data + payload->DataSize;
                while (data < end)
                {
                    std::string path(data);
                    data += path.size() + 1;

                    if (std::filesystem::path(path).extension() == k_GlslExtension)
                        LoadShader(path);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::End();
}

void ShaderEditorWindow::DrawToolbar()
{
    ImGui::Spacing();

    // ---- File name input ----
    ImGui::Text("File:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##ShaderFileName", mFileNameBuffer, sizeof(mFileNameBuffer));

    ImGui::SameLine();
    if (ImGui::Button("New"))
        CreateNewShader();

    ImGui::SameLine();
    if (ImGui::Button("Open"))
        OpenShader();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
    if (ImGui::Button("Save"))
        SaveShader();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("Delete"))
        DeleteShader();
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

void ShaderEditorWindow::CreateNewShader()
{
    strncpy(mFileNameBuffer, "NewShader", sizeof(mFileNameBuffer) - 1);
    mFileNameBuffer[sizeof(mFileNameBuffer) - 1] = '\0';
    mCurrentFilePath.clear();
    mTextEditor.SetText(k_DefaultShaderSource);
    mHasUnsavedChanges = true;
}

void ShaderEditorWindow::OpenShader()
{
    if (mFileNameBuffer[0] == '\0')
    {
        NOUS_WARN("[ShaderEditor] Cannot open: file name is empty.");
        return;
    }

    std::filesystem::path loadPath = std::filesystem::path(k_AssetsShaderPath)
        / (std::string(mFileNameBuffer) + k_GlslExtension);

    LoadShader(loadPath);
}

void ShaderEditorWindow::SaveShader()
{
    if (mFileNameBuffer[0] == '\0')
    {
        NOUS_WARN("[ShaderEditor] Cannot save: file name is empty.");
        return;
    }

    std::filesystem::path savePath = std::filesystem::path(k_AssetsShaderPath)
        / (std::string(mFileNameBuffer) + k_GlslExtension);

    std::ofstream file(savePath);
    if (!file.is_open())
    {
        NOUS_ERROR("[ShaderEditor] Failed to open file for writing: %s", savePath.string().c_str());
        return;
    }

    file << mTextEditor.GetText();
    file.close();

    if (!file)
    {
        NOUS_ERROR("[ShaderEditor] Failed to write to file: %s", savePath.string().c_str());
        return;
    }

    mCurrentFilePath    = savePath;
    mHasUnsavedChanges  = false;
    NOUS_INFO("[ShaderEditor] Saved: %s", savePath.string().c_str());
}

void ShaderEditorWindow::DeleteShader()
{
    if (mCurrentFilePath.empty())
    {
        NOUS_WARN("[ShaderEditor] No shader file loaded to delete.");
        return;
    }

    std::error_code ec;
    if (std::filesystem::remove(mCurrentFilePath, ec))
    {
        NOUS_INFO("[ShaderEditor] Deleted: %s", mCurrentFilePath.string().c_str());
    }
    else
    {
        NOUS_ERROR("[ShaderEditor] Failed to delete: %s (%s)",
            mCurrentFilePath.string().c_str(), ec.message().c_str());
    }

    mCurrentFilePath.clear();
    mFileNameBuffer[0] = '\0';
    mTextEditor.SetText("");
    mHasUnsavedChanges = false;
}

void ShaderEditorWindow::LoadShader(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        NOUS_ERROR("[ShaderEditor] Failed to open shader: %s", filePath.string().c_str());
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

    NOUS_INFO("[ShaderEditor] Loaded: %s", filePath.string().c_str());
}

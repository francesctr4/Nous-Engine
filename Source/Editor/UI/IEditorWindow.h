#pragma once

#include "Editor/EditorContext.h"
#include <imgui.h>
#include <string>
#include <imgui_internal.h>

class IEditorWindow
{
    friend class ModuleEditor;
public:
    explicit IEditorWindow(const char* title, EditorContext* context, bool* p_open = nullptr, const bool start_open = true)
        : editorContext(context), title(title), internal_open(start_open), p_open(p_open ? p_open : &internal_open) {}

    virtual ~IEditorWindow() = default;

    // FINAL shared draw
    void Draw();

    bool IsOpen() const { return *p_open; }
    void Open()  const { *p_open = true;  }
    void Close() const { *p_open = false; }

    const char* GetTitle() const { return title; }
    const EditorContext* GetContext() const { return editorContext; }

protected:
    // Override points
    // NEW Hooks: Standard windows always call End(), MenuBars only if Begin returns true.
    virtual bool Begin(bool& outVisible) {
        outVisible = ImGui::Begin(title, p_open, GetWindowFlags());
        return true; // Standard windows always require an ImGui::End() call
    }

    virtual void End() {
        ImGui::End();
    }

    virtual void Init() {} // Optional
    virtual void Update() {}                       // Runs every frame BEFORE Begin
    virtual void OnLayoutUpdated(const ImVec2&) {} // Optional
    virtual void DrawContent() = 0; // Mandatory
    virtual void FinishUpdate() {} // Optional
    // If true, Update() and OnLayoutUpdated() will run even if the window is collapsed.
    virtual bool UpdatesWhenCollapsed() const { return false; }
    // NEW: Allow derived classes to inject ImGui flags dynamically
    virtual ImGuiWindowFlags GetWindowFlags() const { return 0; }
    virtual void OnFileDrop(const std::string& /*path*/) {}

    const ImVec2& GetContentPos()  const { return contentPos; }
    const ImVec2& GetContentSize() const { return contentSize; }
    const ImVec2& GetContentEnd()  const { return contentEnd; }
    ImRect GetContentRect() const { return ImRect(contentPos, contentEnd); }

    // Shared layout state (usable by all windows)
    ImVec2 contentMin{};
    ImVec2 contentSize{};
    ImVec2 contentPos{};     // absolute screen-space
    ImVec2 contentEnd{};     // absolute screen-space
    bool layoutValid{false};

    EditorContext* editorContext;

    virtual bool UpdateLayout();

private:
    bool initialized{ false };
    const char* title;
    bool internal_open;
    bool* p_open;
};
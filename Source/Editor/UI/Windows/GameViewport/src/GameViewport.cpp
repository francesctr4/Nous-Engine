#include "Editor/UI/Windows/GameViewport/include/GameViewport.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"

GameViewport::GameViewport(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open) {}

void GameViewport::Init()
{
    CreateGameViewportDescriptorSets();
}

bool GameViewport::UpdatesWhenCollapsed() const
{
    return true;
}

void GameViewport::OnLayoutUpdated(const ImVec2& panelSize)
{
    if (ModuleScene* scene = editorContext->GetScene())
        scene->gameViewportAspect = panelSize.x / panelSize.y;
}

bool GameViewport::Begin(bool& outVisible)
{
    const bool ok = IEditorWindow::Begin(outVisible);

    // ModuleEditor resets the gate to disabled each frame; we flip it on only when
    // the GameViewport (or one of its child windows) actually has keyboard focus.
    // Querying after ImGui::Begin works whether the window is visible or collapsed
    // because the window is still on ImGui's stack until End() is called.
    if (ModuleInput* moduleInput = editorContext->GetInput())
    {
        const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (focused)
            moduleInput->SetScriptInputEnabled(true);
    }

    return ok;
}

void GameViewport::DrawContent()
{
    // Constants
    constexpr ImVec2 uvMin(0.0f, 0.0f);
    constexpr ImVec2 uvMax(1.0f, 1.0f);
	constexpr auto backgroundColor = IM_COL32(100, 100, 100, 255);
	constexpr auto borderColor = IM_COL32(255, 255, 255, 255);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(
        contentPos,
        contentEnd,
        backgroundColor
    );

    ModuleScene* scene = editorContext->GetScene();
    if (!scene || !scene->HasMainCamera())
    {
        constexpr auto  textColor = IM_COL32(200, 200, 200, 255);
        constexpr char  message[] = "No game camera in scene";
        constexpr float fontSize  = 20.0f;
        ImFont*         font      = ImGui::GetFont();
        const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, message);
        const ImVec2 textPos(
            contentPos.x + (contentSize.x - textSize.x) * 0.5f,
            contentPos.y + (contentSize.y - textSize.y) * 0.5f
        );
        drawList->AddText(font, fontSize, textPos, textColor, message);

        drawList->AddRect(contentPos, contentEnd, borderColor);
        return;
    }

    // Rendering context
    const VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    // Main image
    ImGui::SetCursorPos(contentMin);
    ImGui::Image(
        NOUS_ImGuiVulkanResources::GetViewportTexture(
            vkContext->imGuiResources
                .m_GameViewportDescriptorSets[vkContext->imageIndex]
        ),
        contentSize,
        uvMin,
        uvMax
    );

    // Overlay / border
    drawList->AddRect(
        contentPos,
        contentEnd,
        borderColor
    );
}

void GameViewport::CreateGameViewportDescriptorSets()
{
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    for (uint32 i = 0; i < vkContext->imGuiResources.m_GameViewportImages.size(); ++i)
    {
        vkContext->imGuiResources.m_GameViewportDescriptorSets[i] = ImGui_ImplVulkan_AddTexture(
                vkContext->imGuiResources.m_GameViewportTextureSampler,
                vkContext->imGuiResources.m_GameViewportImages[i].view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void GameViewport::DestroyGameViewportDescriptorSets()
{
    const VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    for (uint32 i = 0; i < vkContext->imGuiResources.m_GameViewportImages.size(); ++i)
    {
        ImGui_ImplVulkan_RemoveTexture(vkContext->imGuiResources.m_GameViewportDescriptorSets[i]);
    }
}

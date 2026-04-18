#include "Editor/UI/Windows/GameViewport/include/GameViewport.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

GameViewport::GameViewport(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    Init();
}

void GameViewport::Init()
{
    CreateGameViewportDescriptorSets();
}

void GameViewport::Draw()
{
    if (*p_open)
    {
        const bool visible = ImGui::Begin(title, p_open);

        // Read content size regardless of visibility — ImGui preserves the window's
        // size in its internal state even for hidden docked tabs, so this stays
        // accurate during resize without requiring the tab to be active.
        {
            const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
            const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
            const auto squareSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
            if (squareSize.x > 0.0f && squareSize.y > 0.0f)
                editorContext->GetScene()->gameViewportAspect = squareSize.x / squareSize.y;
        }

        if (visible)
        {
            // Get the size of the window's content area
            const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
            const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const auto squareSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
            const auto squarePos = ImVec2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
            const auto squareEnd = ImVec2(squarePos.x + squareSize.x, squarePos.y + squareSize.y);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Draw gray background
            drawList->AddRectFilled(squarePos, squareEnd, IM_COL32(100, 100, 100, 255));

            if (squareSize.x > 0.0f && squareSize.y > 0.0f)
            {
                constexpr ImVec2 uvMin(0.0f, 0.0f);
                constexpr ImVec2 uvMax(1.0f, 1.0f);

                // Position the image at the start of the content region and render
                ImGui::SetCursorPos(contentMin); // Position relative to window's content area

                const VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

                ImGui::Image(
                        NOUS_ImGuiVulkanResources::GetViewportTexture(
                            vkContext->imGuiResources.m_GameViewportDescriptorSets[vkContext->imageIndex]),
                        squareSize, uvMin, uvMax);

                // Draw white border on top
                drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));
            }
        }
        ImGui::End();
    }
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

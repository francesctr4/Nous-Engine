#include "GameViewport.h"

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Engine/Renderer/Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"

GameViewport::GameViewport(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
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
        if (ImGui::Begin(title, p_open))
        {
            // Get the size of the window's content area
            ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
            ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 squareSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);
            ImVec2 squarePos = ImVec2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
            ImVec2 squareEnd = ImVec2(squarePos.x + squareSize.x, squarePos.y + squareSize.y);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Draw gray background
            drawList->AddRectFilled(squarePos, squareEnd, IM_COL32(100, 100, 100, 255));

            // Calculate aspect ratios and UV coordinates
            float textureWidth = 1920.0f;
            float textureHeight = 1080.0f;

            float textureAspect = textureWidth / textureHeight;
            float viewportAspect = squareSize.x / squareSize.y;

            ImVec2 uvMin(0.0f, 0.0f);
            ImVec2 uvMax(1.0f, 1.0f);

            if (viewportAspect < textureAspect)
            {
                // Viewport is narrower: crop left/right
                float cropFactor = textureAspect / viewportAspect;
                uvMin.x = 0.5f - 0.5f / cropFactor;
                uvMax.x = 0.5f + 0.5f / cropFactor;
            }
            else if (viewportAspect > textureAspect)
            {
                // Viewport is wider: crop top/bottom
                float cropFactor = viewportAspect / textureAspect;
                uvMin.y = 0.5f - 0.5f / cropFactor;
                uvMax.y = 0.5f + 0.5f / cropFactor;
            }

            // Position the image at the start of the content region and render
            ImGui::SetCursorPos(contentMin); // Position relative to window's content area

            VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

            ImGui::Image(
                    static_cast<ImTextureID>(
                            NOUS_ImGuiVulkanResources::GetViewportTexture(
                                    vkContext->imGuiResources.m_GameViewportDescriptorSets[vkContext->imageIndex])),
                    squareSize, uvMin, uvMax);

            // Draw white border on top
            drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));
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
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    for (uint32 i = 0; i < vkContext->imGuiResources.m_GameViewportImages.size(); ++i)
    {
        ImGui_ImplVulkan_RemoveTexture(vkContext->imGuiResources.m_GameViewportDescriptorSets[i]);
    }
}

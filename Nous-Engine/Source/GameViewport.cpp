#include "GameViewport.h"

#include "ModuleResourceManager.h"
#include "ModuleCamera3D.h"

#include "VulkanTypes.inl"
#include "VulkanBackend.h"

#include "VulkanImGuiResources.h"

GameViewport::GameViewport(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

void GameViewport::Init()
{
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    NOUS_ImGuiVulkanResources::CreateViewportDescriptorSets(vkContext);
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

            ImGui::Image(NOUS_ImGuiVulkanResources::GetViewportTexture(vkContext, vkContext->imageIndex),
                squareSize, uvMin, uvMax);

            // Draw white border on top
            drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));
        }
        ImGui::End();
    }
}
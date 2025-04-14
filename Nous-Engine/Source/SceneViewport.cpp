#include "SceneViewport.h"

#include "ModuleResourceManager.h"
#include "ModuleCamera3D.h"

#include "VulkanTypes.inl"
#include "VulkanBackend.h"

#include "VulkanImGuiResources.h"

SceneViewport::SceneViewport(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

void SceneViewport::Init()
{
    VulkanContext* vkContext = VulkanBackend::GetVulkanContext();

    NOUS_ImGuiVulkanResources::CreateSceneViewportDescriptorSets(vkContext);
}

void SceneViewport::Draw()
{
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            External->camera->sceneViewportHovered = ImGui::IsWindowHovered();

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

            ImGui::Image(NOUS_ImGuiVulkanResources::GetViewportTexture(
                vkContext->imGuiResources.m_ViewportDescriptorSets[vkContext->imageIndex]),
                squareSize, uvMin, uvMax);

            // Draw white border on top
            drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));

            // Make the entire window area a drag-and-drop target
            ImGui::SetCursorScreenPos(squarePos);          // Position the invisible button to start at the top-left of the content area
            ImGui::InvisibleButton("DropTarget", squareSize); // Create an invisible button that covers the entire content area

            // Start the drag-and-drop target
            if (ImGui::BeginDragDropTarget())
            {
                // Accept the payload
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

                if (payload != NULL)
                {
                    // Process the payload here
                    //std::string assetsFilePathDrop = (const char*)payload->Data;
                    //External->resourceManager->CreateResource(assetsFilePathDrop);

                    //ImGuiID* payload_items = (ImGuiID*)payload->Data;
                    //const int item_count = (int)(payload->DataSize / sizeof(ImGuiID));

                    //// For example, print the dropped item IDs
                    //for (int i = 0; i < item_count; ++i)
                    //{
                    //    ImGui::Text("Dropped item ID: %d", payload_items[i]);
                    //    External->resourceManager->CreateResource(assetsFilePathDrop);
                    //}

                    const char* payload_data = (const char*)payload->Data;
                    std::vector<std::string> filePaths;

                    // Split the payload data into individual file paths (e.g., null-terminated strings)
                    while (*payload_data)
                    {
                        std::string path(payload_data);

                        // Validate the path to ensure it contains only valid ASCII characters
                        if (!path.empty() && IsValidASCII(path))
                        {
                            filePaths.push_back(path);
                        }

                        // Move to the next string (skip the null terminator)
                        payload_data += path.length() + 1;
                    }

                    // Process each file path
                    for (const auto& path : filePaths)
                    {
                        ImGui::Text("Dropped file: %s", path.c_str());

                        External->jobSystem->SubmitJob([path]()
                            {
                                External->resourceManager->CreateResource(path);
                            }, "Create Resource");
                        
                    }

                }

                // End the drag-and-drop target
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::End();
    }
}

// Utility function to check if a string contains only valid ASCII characters.
bool SceneViewport::IsValidASCII(const std::string& str)
{
    for (auto c : str)
    {
        if (static_cast<unsigned char>(c) > 127)
        {
            return false;
        }
    }
    return true;
}
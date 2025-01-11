#include "SceneViewport.h"

#include "ModuleResourceManager.h"

SceneViewport::SceneViewport(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

void SceneViewport::Init()
{

}

void SceneViewport::Draw()
{
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            // Get the size of the window's content area
            ImVec2 contentMin = ImGui::GetWindowContentRegionMin(); // Top-left of content area (relative to window)
            ImVec2 contentMax = ImGui::GetWindowContentRegionMax(); // Bottom-right of content area (relative to window)
            ImVec2 windowPos = ImGui::GetWindowPos();               // Top-left of the window on screen
            ImVec2 squarePos = { windowPos.x + contentMin.x, windowPos.y + contentMin.y };
            ImVec2 squareSize = { contentMax.x - contentMin.x, contentMax.y - contentMin.y };
            ImVec2 squareEnd = { squarePos.x + squareSize.x, squarePos.y + squareSize.y };

            // Draw the square (optional, for visualization)
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(squarePos, squareEnd, IM_COL32(100, 100, 100, 255)); // Gray square
            drawList->AddRect(squarePos, squareEnd, IM_COL32(255, 255, 255, 255));       // White border

            // Draw centered text
            const char* text = "Insert Viewport Here";
            ImVec2 textSize = ImGui::CalcTextSize(text); // Calculate the size of the text
            ImVec2 textPos = {
                squarePos.x + (squareSize.x - textSize.x) * 0.5f, // Center X
                squarePos.y + (squareSize.y - textSize.y) * 0.5f  // Center Y
            };
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text); // White text

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
                        External->resourceManager->CreateResource(path);
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
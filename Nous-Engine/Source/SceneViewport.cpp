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
            // Start the drag-and-drop target
            if (ImGui::BeginDragDropTarget())
            {
                // Accept the payload
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS");

                if (payload != NULL)
                {
                    // Process the payload here
                    std::string assetsFilePathDrop = (const char*)payload->Data;
                    External->resourceManager->CreateResource(assetsFilePathDrop);

                    //ImGuiID* payload_items = (ImGuiID*)payload->Data;
                    //const int item_count = (int)(payload->DataSize / sizeof(ImGuiID));

                    //// For example, print the dropped item IDs
                    //for (int i = 0; i < item_count; ++i)
                    //{
                    //    ImGui::Text("Dropped item ID: %d", payload_items[i]);
                    //    External->resourceManager->CreateResource(assetsFilePathDrop);
                    //}

                    

                }

                // End the drag-and-drop target
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::End();
    }
}


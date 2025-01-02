#include "ResourcesWindow.h"

#include "ModuleResourceManager.h"
#include "Resource.h"

Resources::Resources(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

void Resources::Init()
{

}

void Resources::Draw()
{
    static uint32 previousResourceCount = 0;

    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            std::unordered_map<UID, Resource*> resourcesMap = External->resourceManager->GetResourcesMap();
            uint32 currentResourceCount = resourcesMap.size();

            ImGui::TextColored(
                ImVec4(1.f, 0.5f, 0.5f, 1.f),
                "Total Resources Loaded: %d",
                currentResourceCount
            );

            ImGui::Spacing();

            static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

            if (ImGui::BeginTable("ResourcesTable", 4, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("UID", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("References", ImGuiTableColumnFlags_WidthStretch);

                AlignHeadersToCenter();

                for (const auto& [UID, Resource] : resourcesMap)
                {
                    ImVec4 textColor;
                    ChooseTextColor(Resource->GetType(), textColor);

                    ImGui::TableNextRow();

                    DisplayResource(Resource, textColor);
                }

                // Scroll to the bottom if a new resource is added
                if (currentResourceCount > previousResourceCount)
                {
                    ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndTable();
            }

            // Update the previous resource count
            previousResourceCount = currentResourceCount;
        }
        ImGui::End();
    }
}

// ------------------------------------------------------------------------------------------------------------ //

void Resources::AlignHeadersToCenter()
{
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

    for (int column = 0; column < 4; column++)
    {
        ImGui::TableSetColumnIndex(column);

        const char* header = nullptr;

        if (column == 0) header = "Name";
        if (column == 1) header = "UID";
        if (column == 2) header = "Type";
        if (column == 3) header = "References";

        float headerWidth = ImGui::CalcTextSize(header).x;
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float offsetX = (availableWidth - headerWidth) * 0.5f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::Text("%s", header);
    }
}

void Resources::ChooseTextColor(const ResourceType& type, ImVec4& textColor)
{
    switch (type)
    {
        case ResourceType::MESH:
        {
            textColor = ImVec4(0.0f, 0.8f, 0.5f, 1.0f);
            break;
        }
        case ResourceType::TEXTURE:
        {
            textColor = ImVec4(0.5f, 0.8f, 0.0f, 1.0f);
            break;
        }
        case ResourceType::MATERIAL:
        {
            textColor = ImVec4(0.8f, 0.5f, 0.0f, 1.0f);
            break;
        }
        default:
        {
            textColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            break;
        }
    }
}

void Resources::DisplayResource(const Resource* resource, const ImVec4& textColor)
{
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%s", resource->GetName().c_str());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%d", resource->GetUID());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(2);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%s", Resource::GetStringFromType(resource->GetType()).c_str());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(3);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%d", resource->GetReferenceCount());
    ImGui::PopStyleColor();
}

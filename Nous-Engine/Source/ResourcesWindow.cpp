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
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            ImGui::TextColored(
                ImVec4(1.f, 0.5f, 0.5f, 1.f), 
                "Total Resources Loaded: %d", 
                External->resourceManager->GetResourcesMap().size()
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

                //for (const auto& [UID, Resource] : External->resourceManager->GetResourcesMap())
                //{
                //    ImGui::TableNextRow();

                //    ImGui::TableSetColumnIndex(0);
                //    ImGui::Text("%s", Resource->GetName().c_str());

                //    ImGui::TableSetColumnIndex(1);
                //    ImGui::Text("%d", Resource->GetUID());

                //    ImGui::TableSetColumnIndex(2);
                //    ImGui::Text("%s", Resource::GetStringFromType(Resource->GetType()).c_str());

                //    ImGui::TableSetColumnIndex(3);
                //    ImGui::Text("%d", Resource->GetReferenceCount());
                //}

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
}

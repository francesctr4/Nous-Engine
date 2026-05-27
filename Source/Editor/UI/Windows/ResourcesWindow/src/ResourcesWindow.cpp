#include "Editor/UI/Windows/ResourcesWindow/include/ResourcesWindow.h"

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/TypeRegistry/TypeRegistry.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include <unordered_map>

#include "imgui.h"

Resources::Resources(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void Resources::DrawContent()
{
    std::unordered_map<uint32, Resource*> resourcesMap = editorContext->GetResourceManager()->GetResourcesMap();
    auto currentResourceCount = static_cast<uint32>(resourcesMap.size());

    ImGui::TextColored(
        ImVec4(1.f, 0.5f, 0.5f, 1.f),
        "Total Resources Loaded: %d",
        currentResourceCount
    );

    ImGui::Spacing();

    // Calculate available space after previous widgets
    ImVec2 availableSpace = ImGui::GetContentRegionAvail();

    // Add ScrollY to flags for table scrolling if needed
    static ImGuiTableFlags flags =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable |
        ImGuiTableFlags_ScrollY; // <-- Added ScrollY flag

    if (ImGui::BeginTable("ResourcesTable", 4, flags, ImVec2(0, availableSpace.y)))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("UID", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("References", ImGuiTableColumnFlags_WidthStretch);

        AlignHeadersToCenter();

        for (const auto& [UID, Resource] : resourcesMap)
        {
            if (!Resource) continue; // skip in-progress loads (nullptr placeholder)

            ImVec4 textColor;
            ChooseTextColor(Resource->GetType(), textColor);

            ImGui::TableNextRow();

            DisplayResource(Resource, textColor);
        }

        // Scroll to the bottom if a new resource is added (now affects the table's scroll)
        if (currentResourceCount > previousResourceCount)
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    previousResourceCount = currentResourceCount;
}

// ------------------------------------------------------------------------------------------------------------ //

void Resources::AlignHeadersToCenter() const
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

void Resources::ChooseTextColor(const ResourceType& type, ImVec4& textColor) const
{
    if (const TypeDescriptor* d = GetTypeRegistry().Get(type))
    {
        const auto& c = d->display.color;
        textColor = ImVec4(c[0], c[1], c[2], c[3]);
        return;
    }
    textColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // fallback for unknown / unregistered
}

void Resources::DisplayResource(const Resource* resource, const ImVec4& textColor) const
{
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%s", resource->GetName().c_str());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%u", resource->GetUID());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(2);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    {
        std::string extLabel;
        switch (resource->GetType())
        {
            case ResourceType::SHADER:
            {
                extLabel = "spv";
                break;
            }
            default:
            {
                const std::string ext = nous::engine::filesystem::GetExtension(resource->GetLibraryPath());
                extLabel = ext.empty() ? std::string{} : ext.substr(1);
                break;
            }
        }
        ImGui::Text("%s", extLabel.c_str());
    }
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(3);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%d", resource->GetReferenceCount());
    ImGui::PopStyleColor();
}

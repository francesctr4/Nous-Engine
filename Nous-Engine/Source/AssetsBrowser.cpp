#include "AssetsBrowser.h"
#include "ModuleEditor.h"

#include "FileManager.h"
#include "ModuleFileSystem.h"

static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

AssetsBrowser::AssetsBrowser(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

void AssetsBrowser::Init()
{
    AddItemsFromDirectory("Assets");
}

// Logic would be written in the main code BeginChild() and outputing to local variables.
// We extracted it into a function so we can call it easily from multiple places.

void AssetsBrowser::AddItems(int count)
{
    //if (Items.Size == 0) 
    //{
    //    NextItemId = 0;
    //}
    //    
    //Items.reserve(Items.Size + count);

    //for (int n = 0; n < count; n++, NextItemId++) 
    //{
    //    Items.push_back(ExampleAsset(NextItemId, (NextItemId % 20) < 15 ? 0 : (NextItemId % 20) < 18 ? 1 : 2));
    //}
        
    RequestSort = true;
}

void AssetsBrowser::ClearItems()
{
    Items.clear();
    Selection.Clear();
}

void AssetsBrowser::UpdateLayoutSizes(float avail_width)
{
    // Layout: when not stretching: allow extending into right-most spacing.
    LayoutItemSpacing = (float)IconSpacing;
    if (StretchSpacing == false)
        avail_width += floorf(LayoutItemSpacing * 0.5f);

    // Layout: calculate number of icon per line and number of lines
    LayoutItemSize = ImVec2(floorf(IconSize), floorf(IconSize));
    LayoutColumnCount = NOUS_MathUtils::MAX((int)(avail_width / (LayoutItemSize.x + LayoutItemSpacing)), 1);
    LayoutLineCount = (Items.size() + LayoutColumnCount - 1) / LayoutColumnCount;

    // Layout: when stretching: allocate remaining space to more spacing. Round before division, so item_spacing may be non-integer.
    if (StretchSpacing && LayoutColumnCount > 1)
        LayoutItemSpacing = floorf(avail_width - LayoutItemSize.x * LayoutColumnCount) / LayoutColumnCount;

    LayoutItemStep = ImVec2(LayoutItemSize.x + LayoutItemSpacing, LayoutItemSize.y + LayoutItemSpacing);
    LayoutSelectableSpacing = NOUS_MathUtils::MAX(floorf(LayoutItemSpacing) - IconHitSpacing, 0.0f);
    LayoutOuterPadding = floorf(LayoutItemSpacing * 0.5f);
}
#include <filesystem>

int DetermineTypeFromDirectory(const std::string& directory_name) {
    if (directory_name == "Textures") 
    {
        return 1; // Textures
    }
    else if (directory_name == "Meshes") 
    {
        return 3; // Models
    }
    else if (directory_name == "Materials") 
    {
        return 2; // Audio
    }
    else 
    {
        return 0; // Other
    }
}

void AssetsBrowser::AddItemsFromDirectory(const std::string& directoryPath)
{
    Items.clear();

    for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
    {
        std::string path = entry.path().string();

        // Ignore .meta files
        if (entry.is_regular_file() && entry.path().extension() == ".meta")
        {
            continue; // Skip this file
        }

        if (entry.is_directory())
        {
            const std::string& directoryName = entry.path().filename().string();
            Items.push_back(ExampleAsset(NextItemId++, path, directoryName, FileType::FOLDER)); // Type 3 for directories
        }
        else if (entry.is_regular_file())
        {
            std::string file_extension = entry.path().extension().string();
            std::string file_name = entry.path().filename().string();
            FileType type = DetermineFileType(file_extension);
            Items.push_back(ExampleAsset(NextItemId++, path, file_name, type));
        }
    }

    RequestSort = true;
}

FileType AssetsBrowser::DetermineFileType(const std::string& extension)
{
    // Convert the extension to lowercase to handle case-insensitive extensions.
    std::string lowerExtension = extension;
    std::transform(lowerExtension.begin(), lowerExtension.end(), lowerExtension.begin(), ::tolower);

    // Check if the extension exists in the map.
    auto it = extensionToFileType.find(lowerExtension);
    if (it != extensionToFileType.end())
    {
        return it->second; // Return the corresponding FileType if found.
    }

    return FileType::UNKNOWN; // Return UNKNOWN if the extension is not found.
}

ExampleAsset* AssetsBrowser::GetItemByID(ImGuiID ID)
{
    // Iterate over the vector of ExampleAsset items
    for (auto& item : Items)
    {
        if (item.ID == ID) // Check if the ID matches
        {
            return &item; // Return the pointer to the item if the ID matches
        }
    }
    return nullptr; // Return nullptr if the ID is not found
}

void AssetsBrowser::Draw()
{
    if (*p_open) 
    {
        ImGui::SetNextWindowSize(ImVec2(IconSize * 25, IconSize * 15), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Actions"))
            {
                if (ImGui::MenuItem("Refresh Assets"))
                {
                    AddItemsFromDirectory(current_directory);
                }

                if (ImGui::MenuItem("Regenerate Library"))
                {
                    External->jobSystem->SubmitJob([]() 
                        {
                            NOUS_FileManager::DeleteDirectory("Library");

                            External->fileSystem->CreateLibraryFolder();

                            std::system("compile-shaders.bat");

                            External->fileSystem->ImportDirectory("Assets");
                        }, "Regenerate Library");
                }

                if (ImGui::MenuItem("Add 10000 items"))
                    AddItems(10000);
                if (ImGui::MenuItem("Clear items"))
                    ClearItems();
                ImGui::Separator();
                if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
                    *p_open = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Delete", "Del", false, Selection.Size > 0))
                    RequestDelete = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::PushItemWidth(ImGui::GetFontSize() * 10);

                ImGui::SeparatorText("Contents");
                ImGui::Checkbox("Show Type Overlay", &ShowTypeOverlay);
                ImGui::Checkbox("Allow Sorting", &AllowSorting);

                ImGui::SeparatorText("Selection Behavior");
                ImGui::Checkbox("Allow dragging unselected item", &AllowDragUnselected);
                ImGui::Checkbox("Allow box-selection", &AllowBoxSelect);

                ImGui::SeparatorText("Layout");
                ImGui::SliderFloat("Icon Size", &IconSize, 16.0f, 128.0f, "%.0f");
                ImGui::SameLine(); HelpMarker("Use CTRL+Wheel to zoom");
                ImGui::SliderInt("Icon Spacing", &IconSpacing, 0, 32);
                ImGui::SliderInt("Icon Hit Spacing", &IconHitSpacing, 0, 32);
                ImGui::Checkbox("Stretch Spacing", &StretchSpacing);
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            //for (auto& item : Items) 
            //{
            //    if (item.fileType == FileType::FOLDER) 
            //    { // Directory
            //        if (ImGui::Button(item.name.c_str()))
            //        {
            //            directory_stack.push(current_directory);
            //            current_directory = current_directory + "/" + item.name;
            //            AddItemsFromDirectory(current_directory);
            //        }
            //    }
            //    else 
            //    { // File
            //        ImGui::Text(item.name.c_str());
            //    }
            //}

            //// Create space before the "Regenerate Library" button to align it to the right
            //ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - 113, 0)); // Push dummy space
            //if (ImGui::Button("Regenerate Library"))
            //{
            //    //ImGui::PushItemWidth(ImGui::GetFontSize() * 10);
            //}

            ImGui::EndMenuBar();
        }

        // Show a table with ONLY one header row to showcase the idea/possibility of using this to provide a sorting UI
        if (AllowSorting)
        {
            // Adjusting style settings to minimize spacing
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // Reduce vertical spacing between items

            // You can also try adjusting padding between header and row:
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0)); // Adjust padding for table rows and headers (top-bottom)
            ImGuiTableFlags table_flags_for_sort_specs = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("for_sort_specs_only", 4, table_flags_for_sort_specs, ImVec2(0.0f, ImGui::GetFrameHeight())))
            {
                // Setup columns
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort, 34);
                ImGui::TableSetupColumn(std::format("{}", current_directory).c_str(), ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
                float right_padding = ImGui::GetContentRegionAvail().x; // Available space to the right
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + right_padding - 100);
                ImGui::TableSetupColumn("Sort by Index", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Sort by Type", ImGuiTableColumnFlags_WidthFixed);

                // Headers row
                ImGui::TableHeadersRow();

                // Add the "Back" button in the first column
                ImGui::TableSetColumnIndex(0); // Set focus to the first column
                if (!directory_stack.empty()) {
                    // Render the "Back" button if the directory stack is not empty
                    if (ImGui::Button("Back"))
                    {
                        current_directory = directory_stack.top();  // Go back to the previous directory
                        directory_stack.pop();  // Pop the stack

                        // Refresh the items in the new current directory
                        AddItemsFromDirectory(current_directory);
                    }
                }
                else {
                    // Optionally, change the appearance of the button if it's disabled
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // Darker button background
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // Darker hover background
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // Darker active background

                    // Change the text color to make it look disabled
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Lighter gray text for disabled state

                    // Render the "Back" button with the new text color
                    ImGui::Button("Back");

                    // Restore the button and text style
                    ImGui::PopStyleColor(4);  // Pop all 4 style colors
                }

                if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) 
                {
                    if (sort_specs->SpecsDirty || RequestSort)
                    {
                        ExampleAsset::SortWithSortSpecs(sort_specs, Items.data(), Items.size());
                        sort_specs->SpecsDirty = RequestSort = false;
                    }
                }

                ImGui::EndTable();
            }
            ImGui::PopStyleVar(2);
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowContentSize(ImVec2(0.0f, LayoutOuterPadding + LayoutLineCount * (LayoutItemSize.y + LayoutItemSpacing)));
        if (ImGui::BeginChild("Assets", ImVec2(0.0f, -ImGui::GetTextLineHeightWithSpacing()), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove))
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            const float avail_width = ImGui::GetContentRegionAvail().x;
            UpdateLayoutSizes(avail_width);

            // Calculate and store start position.
            ImVec2 start_pos = ImGui::GetCursorScreenPos();
            start_pos = ImVec2(start_pos.x + LayoutOuterPadding, start_pos.y + LayoutOuterPadding);
            ImGui::SetCursorScreenPos(start_pos);

            // Multi-select
            ImGuiMultiSelectFlags ms_flags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;

            // - Enable box-select (in 2D mode, so that changing box-select rectangle X1/X2 boundaries will affect clipped items)
            if (AllowBoxSelect)
                ms_flags |= ImGuiMultiSelectFlags_BoxSelect2d;

            // - This feature allows dragging an unselected item without selecting it (rarely used)
            if (AllowDragUnselected)
                ms_flags |= ImGuiMultiSelectFlags_SelectOnClickRelease;

            // - Enable keyboard wrapping on X axis
            // (FIXME-MULTISELECT: We haven't designed/exposed a general nav wrapping api yet, so this flag is provided as a courtesy to avoid doing:
            //    ImGui::NavMoveRequestTryWrapping(ImGui::GetCurrentWindow(), ImGuiNavMoveFlags_WrapX);
            // When we finish implementing a more general API for this, we will obsolete this flag in favor of the new system)
            ms_flags |= ImGuiMultiSelectFlags_NavWrapX;

            ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ms_flags, Selection.Size, Items.size());

            // Use custom selection adapter: store ID in selection (recommended)
            Selection.UserData = this;
            Selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self_, int idx) { AssetsBrowser* self = (AssetsBrowser*)self_->UserData; return self->Items[idx].ID; };
            Selection.ApplyRequests(ms_io);

            const bool want_delete = (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) && (Selection.Size > 0)) || RequestDelete;
            const int item_curr_idx_to_focus = want_delete ? Selection.ApplyDeletionPreLoop(ms_io, Items.size()) : -1;
            RequestDelete = false;

            // Push LayoutSelectableSpacing (which is LayoutItemSpacing minus hit-spacing, if we decide to have hit gaps between items)
            // Altering style ItemSpacing may seem unnecessary as we position every items using SetCursorScreenPos()...
            // But it is necessary for two reasons:
            // - Selectables uses it by default to visually fill the space between two items.
            // - The vertical spacing would be measured by Clipper to calculate line height if we didn't provide it explicitly (here we do).
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(LayoutSelectableSpacing, LayoutSelectableSpacing));

            const ImU32 icon_bg_color = ImGui::GetColorU32(IM_COL32(35, 35, 35, 220));
            const ImVec2 icon_type_overlay_size = ImVec2(4.0f, 4.0f);
            const bool display_label = (LayoutItemSize.x >= ImGui::CalcTextSize("999").x);

            const int column_count = LayoutColumnCount;
            ImGuiListClipper clipper;
            clipper.Begin(LayoutLineCount, LayoutItemStep.y);
            if (item_curr_idx_to_focus != -1)
                clipper.IncludeItemByIndex(item_curr_idx_to_focus / column_count); // Ensure focused item line is not clipped.
            if (ms_io->RangeSrcItem != -1)
                clipper.IncludeItemByIndex((int)ms_io->RangeSrcItem / column_count); // Ensure RangeSrc item line is not clipped.
            while (clipper.Step())
            {
                for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; line_idx++)
                {
                    const int item_min_idx_for_current_line = line_idx * column_count;
                    const int item_max_idx_for_current_line = NOUS_MathUtils::MIN((line_idx + 1) * column_count, (int)Items.size());
                    for (int item_idx = item_min_idx_for_current_line; item_idx < item_max_idx_for_current_line; ++item_idx)
                    {
                        ExampleAsset* item_data = &Items[item_idx];
                        ImGui::PushID((int)item_data->ID);

                        // Position item
                        ImVec2 pos = ImVec2(start_pos.x + (item_idx % column_count) * LayoutItemStep.x, start_pos.y + line_idx * LayoutItemStep.y);
                        ImGui::SetCursorScreenPos(pos);

                        ImGui::SetNextItemSelectionUserData(item_idx);
                        bool item_is_selected = Selection.Contains((ImGuiID)item_data->ID);
                        bool item_is_visible = ImGui::IsRectVisible(LayoutItemSize);

                        if (item_data->fileType == FileType::FOLDER)
                        {
                            std::string folder_label = "##folder";
                            if (ImGui::Button(folder_label.c_str(), LayoutItemSize))
                            {
                                // User-defined functionality for folder clicks
                                directory_stack.push(current_directory);
                                current_directory = current_directory + "\\" + item_data->name;
                                AddItemsFromDirectory(current_directory);

                                ImGui::PopID();
                                break;
                            }
                        }
                        else
                        {
                            // Default Selectable behavior for non-folder items
                            ImGui::Selectable("", item_is_selected, ImGuiSelectableFlags_None, LayoutItemSize);
                        }

                        // Update our selection state immediately (without waiting for EndMultiSelect() requests)
                        // because we use this to alter the color of our text/icon.
                        if (ImGui::IsItemToggledSelection())
                            item_is_selected = !item_is_selected;

                        // Focus (for after deletion)
                        if (item_curr_idx_to_focus == item_idx)
                            ImGui::SetKeyboardFocusHere(-1);

                        // Drag and drop
                        if (ImGui::BeginDragDropSource())
                        {
                            //ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEMS", item_data->path.data(), sizeof(item_data->path));

                            //ImGui::Text("Create Resource: %s", item_data->name.c_str());

                            //ImGui::EndDragDropSource();

                            // Create payload with full selection OR single unselected item.
                            // (the later is only possible when using ImGuiMultiSelectFlags_SelectOnClickRelease)
                            //if (ImGui::GetDragDropPayload() == NULL)
                            //{
                            //    ImVector<std::string> payload_items;
                            //    void* it = NULL;
                            //    ImGuiID id = 0;
                            //    if (!item_is_selected)
                            //        payload_items.push_back(item_data->name);
                            //    else
                            //        while (Selection.GetNextSelectedItem(&it, &id))
                            //            payload_items.push_back(item_data->name);
                            //    ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEMS", payload_items.Data, (size_t)payload_items.size_in_bytes());
                            //}

                            // Display payload content in tooltip, by extracting it from the payload data
                            // (we could read from selection, but it is more correct and reusable to read from payload)
                            //const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                            //const int payload_count = (int)payload->DataSize / (int)sizeof(std::string);
                            ////ImGui::Text("%d assets", payload_count);
                            //ImGui::Text("%s", item_data->name.c_str());

                            //ImGui::EndDragDropSource();

                            std::vector<std::string> payload_items;

                            // Check if the item being dragged is selected
                            if (!item_is_selected)
                            {
                                // If the item is not selected, add only this item's path to the payload
                                payload_items.push_back(item_data->path);
                            }
                            else
                            {
                                // If the item is selected, add all selected items to the payload
                                void* it = NULL;
                                ImGuiID id = 0;
                                while (Selection.GetNextSelectedItem(&it, &id))
                                {
                                    ExampleAsset* selected_item = GetItemByID(id); // Retrieve the actual item data
                                    if (selected_item) // Ensure the item data is valid
                                    {
                                        payload_items.push_back(selected_item->path);
                                    }
                                }
                            }

                            // Calculate the total size for the payload, including null terminators for each string
                            size_t payload_size = 0;
                            for (const auto& path : payload_items)
                            {
                                payload_size += path.size() + 1; // Each string has a null terminator
                            }

                            // Allocate memory for the payload data
                            char* payload_data = new char[payload_size];
                            char* write_ptr = payload_data;

                            // Write the strings into the payload data, ensuring null-termination for each string
                            for (const auto& path : payload_items)
                            {
                                memcpy(write_ptr, path.c_str(), path.size()); // Copy the string
                                write_ptr[path.size()] = '\0'; // Null terminate
                                write_ptr += path.size() + 1; // Move pointer to the next free location
                            }

                            // Send the payload
                            ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEMS", payload_data, payload_size);

                            // Display a tooltip with the number of items being dragged and their paths
                            

                            ImGui::BeginTooltip();
                            ImGui::Text("%zu assets selected", payload_items.size());
                            for (const auto& path : payload_items) {
                                ImGui::Text("%s", path.c_str()); // Print each item's path
                            }
                            ImGui::EndTooltip();

                            // Cleanup temporary memory
                            delete[] payload_data;

                            ImGui::EndDragDropSource();
                            
                        }

                        // Render icon (a real app would likely display an image/thumbnail here)
                        // Because we use ImGuiMultiSelectFlags_BoxSelect2d, clipping vertical may occasionally be larger, so we coarse-clip our rendering as well.
                        if (item_is_visible) {
                            ImVec2 box_min(pos.x - 1, pos.y - 1);
                            ImVec2 box_max(box_min.x + LayoutItemSize.x + 2, box_min.y + LayoutItemSize.y + 2);
                            draw_list->AddRectFilled(box_min, box_max, icon_bg_color);

                            if (ShowTypeOverlay && item_data->fileType != FileType::FOLDER) {
                                ImU32 type_col = icon_type_overlay_colors.at(item_data->fileType);
                                // Increase the size of the overlay
                                float overlay_width = icon_type_overlay_size.x * 1.5f;  // Increase width by 1.5 times
                                float overlay_height = icon_type_overlay_size.y * 1.5f; // Increase height by 1.5 times

                                draw_list->AddRectFilled(
                                    ImVec2(box_max.x - 2 - overlay_width, box_min.y + 2),
                                    ImVec2(box_max.x - 2, box_min.y + 2 + overlay_height),
                                    type_col
                                );
                            }

                            // Render title outside of the box (below the icon)
                            if (display_label) {

                                // Calculate the available width for the title (excluding the icon size and padding)
                                float available_width = LayoutItemSize.x + 14; // The width of the item box

                                // Get the text size for the full title
                                ImVec2 text_size = ImGui::CalcTextSize(item_data->name.c_str());

                                // If the text is too wide, calculate how much we need to truncate
                                std::string title = item_data->name;
                                if (text_size.x > available_width)
                                {
                                    // Start with the full string length
                                    int new_length = item_data->name.length();

                                    // Loop to find the maximum length that fits in the available width
                                    while (new_length > 0)
                                    {
                                        // Get the current substring
                                        std::string truncated_title = item_data->name.substr(0, new_length);

                                        // Calculate the size of the truncated string
                                        ImVec2 truncated_text_size = ImGui::CalcTextSize(truncated_title.c_str());

                                        // Check if the truncated string fits in the available width
                                        if (truncated_text_size.x <= available_width)
                                        {
                                            // Once we find a fitting size, break and append ellipsis
                                            title = truncated_title + "...";
                                            break;
                                        }

                                        // Decrease the string length by 1 and try again
                                        new_length--;
                                    }
                                }

                                ImU32 label_col = ImGui::GetColorU32(item_is_selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);

                                // Calculate the position for the label (below the icon box)
                                ImVec2 label_pos = ImVec2(pos.x, pos.y + LayoutItemSize.y + 4);  // Adjust vertical position

                                // Render text with a smaller font
                                ImGui::PushFont(ModuleEditor::fonts[1]); // Use smaller font for title
                                draw_list->AddText(label_pos, label_col, title.c_str());
                                ImGui::PopFont();
                            }
                        }

                        ImGui::PopID();
                    }
                }
            }
            clipper.End();
            ImGui::PopStyleVar(); // ImGuiStyleVar_ItemSpacing

            // Context menu
            if (ImGui::BeginPopupContextWindow())
            {
                ImGui::Text("Selection: %d items", Selection.Size);
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del", false, Selection.Size > 0))
                    RequestDelete = true;
                ImGui::EndPopup();
            }

            ms_io = ImGui::EndMultiSelect();
            Selection.ApplyRequests(ms_io);
            if (want_delete)
                Selection.ApplyDeletionPostLoop(ms_io, Items, item_curr_idx_to_focus);

            // Zooming with CTRL+Wheel
            if (ImGui::IsWindowAppearing())
                ZoomWheelAccum = 0.0f;
            if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsAnyItemActive() == false)
            {
                ZoomWheelAccum += io.MouseWheel;
                if (fabsf(ZoomWheelAccum) >= 1.0f)
                {
                    // Calculate hovered item index from mouse location
                    // FIXME: Locking aiming on 'hovered_item_idx' (with a cool-down timer) would ensure zoom keeps on it.
                    const float hovered_item_nx = (io.MousePos.x - start_pos.x + LayoutItemSpacing * 0.5f) / LayoutItemStep.x;
                    const float hovered_item_ny = (io.MousePos.y - start_pos.y + LayoutItemSpacing * 0.5f) / LayoutItemStep.y;
                    const int hovered_item_idx = ((int)hovered_item_ny * LayoutColumnCount) + (int)hovered_item_nx;
                    //ImGui::SetTooltip("%f,%f -> item %d", hovered_item_nx, hovered_item_ny, hovered_item_idx); // Move those 4 lines in block above for easy debugging

                    // Zoom
                    IconSize *= powf(1.1f, (float)(int)ZoomWheelAccum);
                    IconSize = std::clamp(IconSize, 16.0f, 128.0f);
                    ZoomWheelAccum -= (int)ZoomWheelAccum;
                    UpdateLayoutSizes(avail_width);

                    // Manipulate scroll to that we will land at the same Y location of currently hovered item.
                    // - Calculate next frame position of item under mouse
                    // - Set new scroll position to be used in next ImGui::BeginChild() call.
                    float hovered_item_rel_pos_y = ((float)(hovered_item_idx / LayoutColumnCount) + fmodf(hovered_item_ny, 1.0f)) * LayoutItemStep.y;
                    hovered_item_rel_pos_y += ImGui::GetStyle().WindowPadding.y;
                    float mouse_local_y = io.MousePos.y - ImGui::GetWindowPos().y;
                    ImGui::SetScrollY(hovered_item_rel_pos_y - mouse_local_y);
                }
            }
        }
        ImGui::EndChild();

        ImGui::Text("Selected: %d/%d items", Selection.Size, Items.size());
        ImGui::End();
    }
}
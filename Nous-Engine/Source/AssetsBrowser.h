#pragma once

#include "IEditorWindow.inl"
#include "MathUtils.h"

// Extra functions to add deletion support to ImGuiSelectionBasicStorage
struct ExampleSelectionWithDeletion : ImGuiSelectionBasicStorage
{
    // Find which item should be Focused after deletion.
    // Call _before_ item submission. Retunr an index in the before-deletion item list, your item loop should call SetKeyboardFocusHere() on it.
    // The subsequent ApplyDeletionPostLoop() code will use it to apply Selection.
    // - We cannot provide this logic in core Dear ImGui because we don't have access to selection data.
    // - We don't actually manipulate the ImVector<> here, only in ApplyDeletionPostLoop(), but using similar API for consistency and flexibility.
    // - Important: Deletion only works if the underlying ImGuiID for your items are stable: aka not depend on their index, but on e.g. item id/ptr.
    // FIXME-MULTISELECT: Doesn't take account of the possibility focus target will be moved during deletion. Need refocus or scroll offset.
    int ApplyDeletionPreLoop(ImGuiMultiSelectIO* ms_io, int items_count)
    {
        if (Size == 0)
            return -1;

        // If focused item is not selected...
        const int focused_idx = (int)ms_io->NavIdItem;  // Index of currently focused item
        if (ms_io->NavIdSelected == false)  // This is merely a shortcut, == Contains(adapter->IndexToStorage(items, focused_idx))
        {
            ms_io->RangeSrcReset = true;    // Request to recover RangeSrc from NavId next frame. Would be ok to reset even when NavIdSelected==true, but it would take an extra frame to recover RangeSrc when deleting a selected item.
            return focused_idx;             // Request to focus same item after deletion.
        }

        // If focused item is selected: land on first unselected item after focused item.
        for (int idx = focused_idx + 1; idx < items_count; idx++)
            if (!Contains(GetStorageIdFromIndex(idx)))
                return idx;

        // If focused item is selected: otherwise return last unselected item before focused item.
        for (int idx = NOUS_MathUtils::MIN(focused_idx, items_count) - 1; idx >= 0; idx--)
            if (!Contains(GetStorageIdFromIndex(idx)))
                return idx;

        return -1;
    }

    // Rewrite item list (delete items) + update selection.
    // - Call after EndMultiSelect()
    // - We cannot provide this logic in core Dear ImGui because we don't have access to your items, nor to selection data.
    template<typename ITEM_TYPE>
    void ApplyDeletionPostLoop(ImGuiMultiSelectIO* ms_io, std::vector<ITEM_TYPE>& items, int item_curr_idx_to_select)
    {
        // Rewrite item list (delete items) + convert old selection index (before deletion) to new selection index (after selection).
        // If NavId was not part of selection, we will stay on the same item.
        std::vector<ITEM_TYPE> new_items;
        new_items.reserve(items.size() - Size);
        int item_next_idx_to_select = -1;

        for (size_t idx = 0; idx < items.size(); ++idx)
        {
            if (!Contains(GetStorageIdFromIndex(idx)))
                new_items.push_back(std::move(items[idx])); // Use std::move for efficiency if ITEM_TYPE is movable
            if (item_curr_idx_to_select == static_cast<int>(idx))
                item_next_idx_to_select = static_cast<int>(new_items.size() - 1);
        }

        items.swap(new_items);

        // Update selection
        Clear();
        if (item_next_idx_to_select != -1 && ms_io->NavIdSelected)
            SetItemSelected(GetStorageIdFromIndex(item_next_idx_to_select), true);
    }
};

enum class FileType 
{
    UNKNOWN = -1,

    FOLDER,
    META,
    MODEL,
    TEXTURE,
    MATERIAL,
    SHADER,
    FONT,
    SCENE,

    ALL_TYPES
};

const std::unordered_map<std::string, FileType> extensionToFileType =
{
    // Model file extensions
    {".fbx", FileType::MODEL},
    {".obj", FileType::MODEL},
    {".dae", FileType::MODEL},

    // Texture file extensions
    {".png", FileType::TEXTURE},
    {".jpg", FileType::TEXTURE},
    {".jpeg", FileType::TEXTURE},
    {".tga", FileType::TEXTURE},
    {".dds", FileType::TEXTURE},

    // Material file extensions
    {".nmat", FileType::MATERIAL},

    // Shader file extensions
    {".glsl", FileType::SHADER},
    {".spv", FileType::SHADER},

    // Font file extensions
    {".ttf", FileType::FONT}, 

    // Scene file extensions
    {".nous", FileType::SCENE},

    // Meta file extensions
    {".meta", FileType::META},
};

// Rendering parameters
const std::unordered_map<FileType, uint32> icon_type_overlay_colors =
{
    {FileType::UNKNOWN, IM_COL32(204, 204, 204, 255)}, // Gray for unknown files

    {FileType::TEXTURE, IM_COL32(127, 204, 0, 255)},   // Green for textures
    {FileType::MATERIAL, IM_COL32(204, 127, 0, 255)},  // Orange for materials
    {FileType::MODEL, IM_COL32(0, 204, 127, 255)},     // Teal for models

    {FileType::META, IM_COL32(255, 255, 255, 255)},    // White for meta
    {FileType::FONT, IM_COL32(127, 0, 255, 255)},      // Purple for fonts
    {FileType::SCENE, IM_COL32(255, 0, 0, 255)},       // Red for scenes
    {FileType::SHADER, IM_COL32(255, 127, 255, 255)},  // Pink for shaders

    {FileType::FOLDER, IM_COL32(255, 204, 0, 255)}     // Yellow for folders
};

struct ExampleAsset
{
    ImGuiID ID;
    std::string name; // Asset's title (file name)
    std::string path;
    FileType fileType;

    ExampleAsset(ImGuiID ID, std::string path, std::string name, FileType fileType = FileType::UNKNOWN)
        : ID(ID), path(path), name(name), fileType(fileType) {}

    static const ImGuiTableSortSpecs* s_current_sort_specs;

#pragma region ASSET SORTING

    static void SortWithSortSpecs(ImGuiTableSortSpecs* sort_specs, ExampleAsset* items, int items_count)
    {
        s_current_sort_specs = sort_specs; // Store in variable accessible by the sort function.
        if (items_count > 1)
            qsort(items, (size_t)items_count, sizeof(items[0]), ExampleAsset::CompareWithSortSpecs);
        s_current_sort_specs = NULL;
    }

    // Compare function to be used by qsort()
    static int CompareWithSortSpecs(const void* lhs, const void* rhs)
    {
        const ExampleAsset* a = (const ExampleAsset*)lhs;
        const ExampleAsset* b = (const ExampleAsset*)rhs;
        for (int n = 0; n < s_current_sort_specs->SpecsCount; n++)
        {
            const ImGuiTableColumnSortSpecs* sort_spec = &s_current_sort_specs->Specs[n];
            int delta = 0;
            if (sort_spec->ColumnIndex == 0)
                delta = ((int)a->ID - (int)b->ID);
            else if (sort_spec->ColumnIndex == 1)
                delta = (static_cast<int>(a->fileType) - static_cast<int>(b->fileType));
            if (delta > 0)
                return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
            if (delta < 0)
                return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
        }
        return ((int)a->ID - (int)b->ID);
    }

#pragma endregion

};

class AssetsBrowser : public IEditorWindow
{
public:
    // Options
    bool            ShowTypeOverlay = true;
    bool            AllowSorting = true;
    bool            AllowDragUnselected = true;
    bool            AllowBoxSelect = true;
    float           IconSize = 100.0f;
    int             IconSpacing = 20;
    int             IconHitSpacing = 14;         // Increase hit-spacing if you want to make it possible to clear or box-select from gaps. Some spacing is required to able to amend with Shift+box-select. Value is small in Explorer.
    bool            StretchSpacing = true;

    // State
    std::vector<ExampleAsset> Items;               // Our items
    ExampleSelectionWithDeletion Selection;     // Our selection (ImGuiSelectionBasicStorage + helper funcs to handle deletion)
    ImGuiID         NextItemId = 0;             // Unique identifier when creating new items
    bool            RequestDelete = false;      // Deferred deletion request
    bool            RequestSort = false;        // Deferred sort request
    float           ZoomWheelAccum = 0.0f;      // Mouse wheel accumulator to handle smooth wheels better

    // Calculated sizes for layout, output of UpdateLayoutSizes(). Could be locals but our code is simpler this way.
    ImVec2          LayoutItemSize;
    ImVec2          LayoutItemStep;             // == LayoutItemSize + LayoutItemSpacing
    float           LayoutItemSpacing = 0.0f;
    float           LayoutSelectableSpacing = 0.0f;
    float           LayoutOuterPadding = 0.0f;
    int             LayoutColumnCount = 0;
    int             LayoutLineCount = 0;

    explicit AssetsBrowser(const char* title, bool start_open = true);
    void Init() override;
    void Draw() override;

    void AddItems(int count);
    void ClearItems();

    // Logic would be written in the main code BeginChild() and outputing to local variables.
    // We extracted it into a function so we can call it easily from multiple places.
    void UpdateLayoutSizes(float avail_width);

    void AddItemsFromDirectory(const std::string& directoryPath);
    FileType DetermineFileType(const std::string& extension);
    ExampleAsset* GetItemByID(ImGuiID ID);

    std::string current_directory = "Assets";
    std::stack<std::string> directory_stack;
};
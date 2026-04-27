#pragma once

#include "Editor/UI/IEditorWindow.h"
#include "Engine/Utils/Math/MathUtils.h"
#include "Engine/Core/Globals.h"

#include <atomic>
#include <string>
#include <stack>
#include <utility>
#include <vector>
#include <thread>
#include <mutex>

#include "imgui.h"

// Extends ImGuiSelectionBasicStorage with deletion helpers for multi-select.
struct MultiSelectStorage : ImGuiSelectionBasicStorage
{
    // Returns the index to focus after deletion (call BEFORE item submission).
    int ApplyDeletionPreLoop(ImGuiMultiSelectIO* ms_io, const int items_count)
    {
        if (Size == 0)
            return -1;

        const int focused_idx = static_cast<int>(ms_io->NavIdItem);
        if (ms_io->NavIdSelected == false)
        {
            ms_io->RangeSrcReset = true;
            return focused_idx;
        }

        for (int idx = focused_idx + 1; idx < items_count; idx++)
            if (!Contains(GetStorageIdFromIndex(idx)))
                return idx;

        for (int idx = NOUS_MathUtils::MIN(focused_idx, items_count) - 1; idx >= 0; idx--)
            if (!Contains(GetStorageIdFromIndex(idx)))
                return idx;

        return -1;
    }

    // Rewrites the item list removing selected entries; updates selection.
    // Call after EndMultiSelect().
    template<typename ITEM_TYPE>
    void ApplyDeletionPostLoop(ImGuiMultiSelectIO* ms_io, std::vector<ITEM_TYPE>& items, const int item_curr_idx_to_select)
    {
        std::vector<ITEM_TYPE> new_items;
        new_items.reserve(items.size() - Size);
        int item_next_idx_to_select = -1;

        for (size_t idx = 0; idx < items.size(); ++idx)
        {
            if (!Contains(GetStorageIdFromIndex(static_cast<int>(idx))))
                new_items.push_back(std::move(items[idx]));
            if (item_curr_idx_to_select == static_cast<int>(idx))
                item_next_idx_to_select = static_cast<int>(new_items.size() - 1);
        }

        items.swap(new_items);

        Clear();
        if (item_next_idx_to_select != -1 && ms_io->NavIdSelected)
            SetItemSelected(GetStorageIdFromIndex(item_next_idx_to_select), true);
    }
};

enum class FileType : int8_t
{
    UNKNOWN = -1,

    FOLDER,
    META,
    MODEL,
    TEXTURE,
    MATERIAL,
    SHADER,
    SCRIPT,
    FONT,
    SCENE,
    PREFAB,

    ALL_TYPES
};

// Maps, icons, and colors are defined in AssetsBrowser.cpp to avoid ODR issues.

struct AssetEntry
{
    ImGuiID     ID;
    std::string name;
    std::string path;
    FileType    fileType;

    AssetEntry(const ImGuiID ID, std::string path, std::string name, const FileType fileType = FileType::UNKNOWN)
        : ID(ID), name(std::move(name)), path(std::move(path)), fileType(fileType) {}
};

class AssetsBrowser : public IEditorWindow
{
public:
    // Options
    bool  ShowTypeOverlay    = true;
    bool  AllowSorting       = true;
    bool  AllowDragUnselected = true;
    bool  AllowBoxSelect     = true;
    float IconSize           = 100.0f;
    int   IconSpacing        = 20;
    int   IconHitSpacing     = 14;
    bool  StretchSpacing     = true;

    // State
    std::vector<AssetEntry> Items;
    MultiSelectStorage      Selection;
    ImGuiID                 NextItemId      = 0;
    bool                    RequestDelete   = false;
    bool                    RequestSort     = false;
    float                   ZoomWheelAccum  = 0.0f;

    // Layout (computed by UpdateLayoutSizes)
    ImVec2 LayoutItemSize;
    ImVec2 LayoutItemStep;
    float  LayoutItemSpacing      = 0.0f;
    float  LayoutSelectableSpacing = 0.0f;
    float  LayoutOuterPadding     = 0.0f;
    int    LayoutColumnCount      = 0;
    int    LayoutLineCount        = 0;

    explicit AssetsBrowser(const char* title, EditorContext* context, bool start_open = true);
    ~AssetsBrowser() override;

    void Init() override;
    void Update() override;
    void DrawContent() override;
    ImGuiWindowFlags GetWindowFlags() const override;

    void OnFileDrop(const std::string& path) override;

    void ClearItems();
    void UpdateLayoutSizes(float avail_width);
    void AddItemsFromDirectory(const std::string& directoryPath);
    FileType DetermineFileType(const std::string& extension);
    AssetEntry* GetItemByID(ImGuiID ID);

    std::string              current_directory = "Assets";
    std::stack<std::string>  directory_stack;

    // Directory hot-reload watcher thread
    void StartDirectoryWatcher();
    void StopDirectoryWatcher();

    std::thread        m_pollThread;
    std::atomic<bool>  m_pollThreadStop { false };
    std::atomic<bool>  m_dirChanged     { false };
    std::mutex         m_watchedDirMutex;
    std::string        m_watchedDir;

    // Library regeneration state
    std::atomic<bool>  m_isRegeneratingLibrary { false };

    // Script creation popup state
    bool show_create_script_popup = false;
    char script_name_buffer[128]  = "";
    std::string script_creation_path;

    // Folder creation popup state
    bool show_create_folder_popup = false;
    char folder_name_buffer[128]  = "";

private:
    void MoveAsset(const std::string& srcPath, const std::string& destDir);
    void DeleteAsset(const std::string& assetPath);
    void ImportExternalFile(const std::string& srcPath);

    std::vector<std::pair<std::string, std::string>> m_pendingMoves;
};

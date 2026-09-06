#include <EditorUI/AssetsBrowser.h>
#include <ModuleEditor/ModuleEditor.h>
#include <EditorUI/TextEditorWindow.h>

#include <algorithm>
#include <cstdio>
#include <format>
#include <cmath>
#include <chrono>

#include <FileSystem/FileSystem.h>
#include <ModuleResourceManager/ModuleResourceManager.h>
#include <ResourceManager/Runtime/ImportPipeline.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>
#include <Scripting/ScriptManager.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>
#include <Logger/Logger.h>

static const std::unordered_map<std::string, FileType> extensionToFileType =
{
    {".fbx",     FileType::MODEL},
    {".obj",     FileType::MODEL},
    {".dae",     FileType::MODEL},
    {".glb",     FileType::MODEL},
    {".gltf",    FileType::MODEL},

    {".png",     FileType::TEXTURE},
    {".jpg",     FileType::TEXTURE},
    {".jpeg",    FileType::TEXTURE},
    {".tga",     FileType::TEXTURE},
    {".dds",     FileType::TEXTURE},

    {".nmat",    FileType::MATERIAL},

    {".cpp",     FileType::SCRIPT},
    {".h",       FileType::SCRIPT},

    {".glsl",    FileType::SHADER},
    {".spv",     FileType::SHADER},

    {".ttf",     FileType::FONT},

    {".nous",    FileType::SCENE},

    {".nprefab", FileType::PREFAB},

    {".meta",    FileType::META},

    {".ogg",    FileType::MUSIC},
    {".wav",    FileType::SFX},

    {".mp4",    FileType::VIDEO},
    {".gif",    FileType::GIF},

    {".nafx",   FileType::AUDIO_GRAPH},

    // Sibling stubs emitted beside a model by ModelParser: one .nskel for the rig,
    // one .nanim per clip. They share the model stem, so distinct icons are the
    // only thing separating them at a glance in the browser.
    {".nskel",  FileType::SKELETON},
    {".nanim",  FileType::ANIMATION}
};

static const std::unordered_map<FileType, const char*> icon_type_glyphs =
{
    {FileType::UNKNOWN,  "\xEF\x85\x9B"},   // U+F15B fa-file
    {FileType::FOLDER,   "\xEF\x81\xBB"},   // U+F07B fa-folder
    {FileType::META,     "\xEF\x81\x9A"},   // U+F05A fa-info-circle
    {FileType::MODEL,    "\xEF\x86\xB2"},   // U+F1B2 fa-cube
    {FileType::TEXTURE,  "\xEF\x80\xBE"},   // U+F03E fa-image
    {FileType::MATERIAL, "\xEF\x94\xBF"},   // U+F53F fa-palette
    {FileType::SHADER,   "\xEF\x9B\xBC"},   // U+F6FC fa-mountain
    {FileType::SCRIPT,   "\xEF\x87\x89"},   // U+F1C9 fa-file-code
    {FileType::FONT,     "\xEF\x80\xB1"},   // U+F031 fa-font
    {FileType::SCENE,    "\xEF\x89\xB9"},   // U+F279 fa-map
    {FileType::PREFAB,   "\xEF\x86\xB3"},   // U+F1B3 fa-cubes
    {FileType::MUSIC,    "\xEF\x80\x81"},   // U+F001 fa-music
    {FileType::SFX,      "\xEF\x80\xA8"},   // U+F028 fa-volume-up
    {FileType::VIDEO,    "\xEF\x80\xBD"},   // U+F03D fa-video
    {FileType::GIF,      "\xEF\x80\x88"},   // U+F008 fa-film
    {FileType::AUDIO_GRAPH, "\xEF\x87\x9E"},// U+F1DE fa-sliders
    {FileType::SKELETON,    "\xEF\x97\x97"},// U+F5D7 fa-bone
    {FileType::ANIMATION,   "\xEF\x9C\x8C"},// U+F70C fa-running
};

static const std::unordered_map<FileType, uint32_t> icon_type_overlay_colors =
{
    {FileType::UNKNOWN,  IM_COL32(204, 204, 204, 255)},
    {FileType::TEXTURE,  IM_COL32(127, 204,   0, 255)},
    {FileType::MATERIAL, IM_COL32(204, 127,   0, 255)},
    {FileType::MODEL,    IM_COL32(  0, 204, 127, 255)},
    {FileType::META,     IM_COL32(255, 255, 255, 255)},
    {FileType::FONT,     IM_COL32(127,   0, 255, 255)},
    {FileType::SCENE,    IM_COL32(255,   0,   0, 255)},
    {FileType::PREFAB,   IM_COL32(100, 180, 255, 255)},
    {FileType::SHADER,   IM_COL32(255, 127, 255, 255)},
    {FileType::SCRIPT,   IM_COL32(  0, 200, 255, 255)},
    {FileType::FOLDER,   IM_COL32(255, 204,   0, 255)},
    {FileType::MUSIC,    IM_COL32(236,  72, 153, 255)},
    {FileType::SFX,      IM_COL32( 56, 218, 191, 255)},
    {FileType::VIDEO,    IM_COL32(153,  76, 237, 255)},   // purple — matches the Video registry asset color
    {FileType::GIF,      IM_COL32( 46, 204, 113, 255)},   // emerald — distinct from the purple .mp4 sibling
    {FileType::AUDIO_GRAPH, IM_COL32(51, 217, 217, 255)}, // cyan — matches the AUDIO_GRAPH registry asset color
    {FileType::SKELETON,    IM_COL32(232, 220, 184, 255)}, // ivory: bone; distinct from META pure white
    {FileType::ANIMATION,   IM_COL32(244, 114,  92, 255)}, // coral: distinct from its .nskel sibling and SCENE red
};

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

AssetsBrowser::AssetsBrowser(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

AssetsBrowser::~AssetsBrowser()
{
    StopDirectoryWatcher();
}

void AssetsBrowser::Init()
{
    AddItemsFromDirectory(current_directory);
    StartDirectoryWatcher();
}

void AssetsBrowser::Update()
{
    if (m_dirChanged.exchange(false, std::memory_order_acquire))
        AddItemsFromDirectory(current_directory);

    ImGui::SetNextWindowSize(ImVec2(IconSize * 25, IconSize * 15), ImGuiCond_FirstUseEver);
}

ImGuiWindowFlags AssetsBrowser::GetWindowFlags() const
{
    return ImGuiWindowFlags_MenuBar;
}

void AssetsBrowser::StartDirectoryWatcher()
{
    if (m_pollThread.joinable())
        return;

    m_pollThreadStop.store(false);
    m_pollThread = std::thread([this]()
    {
        using namespace std::chrono_literals;
        std::filesystem::file_time_type last{};
        std::string currentWatched;

        while (!m_pollThreadStop.load(std::memory_order_relaxed))
        {
            {
                std::lock_guard<std::mutex> lk(m_watchedDirMutex);
                if (m_watchedDir != currentWatched)
                {
                    currentWatched = m_watchedDir;
                    last = {};
                }
            }

            if (!currentWatched.empty())
            {
                std::error_code ec;
                auto writeTime = std::filesystem::last_write_time(currentWatched, ec);
                if (!ec && writeTime != last)
                {
                    if (last != std::filesystem::file_time_type{})
                        m_dirChanged.store(true, std::memory_order_release);
                    last = writeTime;
                }
            }

            for (int i = 0; i < 10 && !m_pollThreadStop.load(std::memory_order_relaxed); ++i)
                std::this_thread::sleep_for(50ms);
        }
    });
}

void AssetsBrowser::StopDirectoryWatcher()
{
    m_pollThreadStop.store(true);
    if (m_pollThread.joinable())
        m_pollThread.join();
}

void AssetsBrowser::ClearItems()
{
    Items.clear();
    Selection.Clear();
}

void AssetsBrowser::UpdateLayoutSizes(float avail_width)
{
    LayoutItemSpacing = static_cast<float>(IconSpacing);
    if (StretchSpacing == false)
        avail_width += floorf(LayoutItemSpacing * 0.5f);

    LayoutItemSize = ImVec2(floorf(IconSize), floorf(IconSize));
    LayoutColumnCount = NOUS_MathUtils::MAX(static_cast<int>(avail_width / (LayoutItemSize.x + LayoutItemSpacing)), 1);
    LayoutLineCount = (static_cast<int>(Items.size()) + LayoutColumnCount - 1) / LayoutColumnCount;

    if (StretchSpacing && LayoutColumnCount > 1)
        LayoutItemSpacing = floorf(avail_width - LayoutItemSize.x * LayoutColumnCount) / LayoutColumnCount;

    LayoutItemStep = ImVec2(LayoutItemSize.x + LayoutItemSpacing, LayoutItemSize.y + LayoutItemSpacing);
    LayoutSelectableSpacing = NOUS_MathUtils::MAX(floorf(LayoutItemSpacing) - static_cast<float>(IconHitSpacing), 0.0f);
    LayoutOuterPadding = floorf(LayoutItemSpacing * 0.5f);
}

void AssetsBrowser::AddItemsFromDirectory(const std::string& directoryPath)
{
    Items.clear();

    {
        std::lock_guard<std::mutex> lk(m_watchedDirMutex);
        m_watchedDir = directoryPath;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
    {
        const std::string path = entry.path().generic_string();

        if (entry.is_regular_file() && entry.path().extension() == ".meta")
            continue;

        if (entry.is_directory())
        {
            const std::string dirName = entry.path().filename().string();
            Items.emplace_back(NextItemId++, path, dirName, FileType::FOLDER);
        }
        else if (entry.is_regular_file())
        {
            const std::string ext  = entry.path().extension().string();
            const std::string name = entry.path().filename().string();
            Items.emplace_back(NextItemId++, path, name, DetermineFileType(ext));
        }
    }

    RequestSort = true;
}

FileType AssetsBrowser::DetermineFileType(const std::string& extension)
{
    std::string lower = extension;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto it = extensionToFileType.find(lower);
    return (it != extensionToFileType.end()) ? it->second : FileType::UNKNOWN;
}

AssetEntry* AssetsBrowser::GetItemByID(ImGuiID ID)
{
    for (auto& item : Items)
    {
        if (item.ID == ID)
            return &item;
    }
    return nullptr;
}

void AssetsBrowser::MoveAsset(const std::string& srcPath, const std::string& destDir)
{
    const std::string filename = std::filesystem::path(srcPath).filename().string();
    const std::string destPath = destDir + "/" + filename;

    if (!nous::engine::filesystem::MoveFile(srcPath, destPath))
    {
        NOUS_ERROR("[AssetsBrowser] Failed to move '%s' to '%s'", srcPath.c_str(), destPath.c_str());
        return;
    }

    const std::string srcMeta  = srcPath  + ".meta";
    const std::string destMeta = destPath + ".meta";
    if (std::filesystem::exists(srcMeta))
    {
        if (!nous::engine::filesystem::MoveFile(srcMeta, destMeta))
        {
            NOUS_ERROR("[AssetsBrowser] Failed to move meta '%s'", srcMeta.c_str());
            return;
        }

        JsonObject root = JsonFile::LoadFromFile(destMeta);
        root.Set("Assets Path", destPath);
        JsonFile::SaveToFile(root, destMeta);

        // Keep in-memory ResourceManager path in sync for the current session.
        MetaFileData meta;
        if (ImportPipeline::GetAssetMetaData(destPath, meta))
        {
            editorContext->GetResourceManager()->UpdateResourcePath(meta.uid, destPath);

            // If this is a shader, re-register the file watcher so hot-reload keeps working.
            if (std::filesystem::path(destPath).extension() == ".glsl")
                editorContext->UpdateShaderWatcherPath(srcPath, destPath);
        }
    }

    AddItemsFromDirectory(current_directory);
}

void AssetsBrowser::DeleteAsset(const std::string& assetPath)
{
    if (std::filesystem::is_directory(assetPath))
    {
        nous::engine::filesystem::DeleteDirectory(assetPath);
        AddItemsFromDirectory(current_directory);
        return;
    }

    MetaFileData meta;
    const bool hasMeta = ImportPipeline::GetAssetMetaData(assetPath, meta);

    nous::engine::filesystem::DeleteFile(assetPath);

    const std::string metaPath = assetPath + ".meta";
    if (std::filesystem::exists(metaPath))
        nous::engine::filesystem::DeleteFile(metaPath);

    if (hasMeta && !meta.libraryPath.empty())
    {
        if (std::filesystem::is_directory(meta.libraryPath))
            nous::engine::filesystem::DeleteDirectory(meta.libraryPath);
        else
            nous::engine::filesystem::DeleteFile(meta.libraryPath);
    }

    AddItemsFromDirectory(current_directory);
}

void AssetsBrowser::ImportExternalFile(const std::string& srcPath)
{
    const std::filesystem::path p(srcPath);
    const std::string destPath = current_directory + "/" + p.filename().string();

    if (!nous::engine::filesystem::CopyFile(srcPath, destPath))
    {
        NOUS_ERROR("[AssetsBrowser] Failed to copy '%s' to '%s'", srcPath.c_str(), destPath.c_str());
        return;
    }

    editorContext->GetResourceManager()->ImportFile(destPath);
    AddItemsFromDirectory(current_directory);
}

void AssetsBrowser::OnFileDrop(const std::string& path)
{
    ImportExternalFile(path);
}

void AssetsBrowser::DrawContent()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Actions"))
        {
            if (ImGui::MenuItem("Refresh Assets"))
            {
                editorContext->GetJobSystem()->SubmitJob([this]()
                {
                    std::system("cmake --build ./ --target CopyAssets");
                    AddItemsFromDirectory(current_directory);
                }, "Refresh Assets");
            }

            {
                const bool isRunning = m_isRegeneratingLibrary.load();
                ImGui::BeginDisabled(isRunning);
                if (ImGui::MenuItem(isRunning ? "Regenerating Library..." : "Regenerate Library"))
                {
                    m_isRegeneratingLibrary = true;
                    auto* resourceManager = editorContext->GetResourceManager();
                    editorContext->GetJobSystem()->SubmitJob([this, resourceManager]()
                    {
                        resourceManager->RegenerateLibrary();
                        m_isRegeneratingLibrary = false;
                        m_dirChanged.store(true, std::memory_order_release);
                    }, "Regenerate Library");
                }
                ImGui::EndDisabled();
            }

            if (ImGui::MenuItem("Clear items"))
                ClearItems();
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
            ImGui::SameLine();
            HelpMarker("Use CTRL+Wheel to zoom");
            ImGui::SliderInt("Icon Spacing", &IconSpacing, 0, 32);
            ImGui::SliderInt("Icon Hit Spacing", &IconHitSpacing, 0, 32);
            ImGui::Checkbox("Stretch Spacing", &StretchSpacing);
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (AllowSorting)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0));

        ImGuiTableFlags table_flags_for_sort_specs = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("for_sort_specs_only", 4, table_flags_for_sort_specs,
                              ImVec2(0.0f, ImGui::GetFrameHeight())))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort, 34);
            ImGui::TableSetupColumn(current_directory.c_str(),
                                    ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Sort by Index", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Sort by Type",  ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::BeginDisabled(directory_stack.empty());
            if (ImGui::Button("Back"))
            {
                current_directory = directory_stack.top();
                directory_stack.pop();
                AddItemsFromDirectory(current_directory);
            }
            ImGui::EndDisabled();

            if (!directory_stack.empty() && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
                {
                    const char* data = static_cast<const char*>(payload->Data);
                    const char* end  = data + payload->DataSize;
                    while (data < end)
                    {
                        std::string srcPath(data);
                        if (!srcPath.empty())
                            m_pendingMoves.emplace_back(srcPath, directory_stack.top());
                        data += srcPath.size() + 1;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs())
            {
                if (sort_specs->SpecsDirty || RequestSort)
                {
                    ImGuiTableSortSpecs* specs = sort_specs;
                    std::sort(Items.begin(), Items.end(), [specs](const AssetEntry& a, const AssetEntry& b)
                    {
                        for (int n = 0; n < specs->SpecsCount; ++n)
                        {
                            const ImGuiTableColumnSortSpecs* s = &specs->Specs[n];
                            int delta = 0;
                            if (s->ColumnIndex == 0)
                                delta = static_cast<int>(a.ID) - static_cast<int>(b.ID);
                            else if (s->ColumnIndex == 1)
                                delta = static_cast<int>(a.fileType) - static_cast<int>(b.fileType);
                            if (delta != 0)
                                return s->SortDirection == ImGuiSortDirection_Ascending ? delta < 0 : delta > 0;
                        }
                        return a.ID < b.ID;
                    });
                    sort_specs->SpecsDirty = RequestSort = false;
                }
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar(2);
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowContentSize(
        ImVec2(0.0f, LayoutOuterPadding + LayoutLineCount * (LayoutItemSize.y + LayoutItemSpacing)));
    float width = ImGui::GetContentRegionAvail().x;
    if (width < 1.0f) width = 1.0f;
    if (ImGui::BeginChild("Assets", ImVec2(0.0f, -ImGui::GetTextLineHeightWithSpacing()), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoMove))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        const float avail_width = ImGui::GetContentRegionAvail().x;
        UpdateLayoutSizes(avail_width);

        ImVec2 start_pos = ImGui::GetCursorScreenPos();
        start_pos = ImVec2(start_pos.x + LayoutOuterPadding, start_pos.y + LayoutOuterPadding);
        ImGui::SetCursorScreenPos(start_pos);

        ImGuiMultiSelectFlags ms_flags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
        if (AllowBoxSelect)
            ms_flags |= ImGuiMultiSelectFlags_BoxSelect2d;
        if (AllowDragUnselected)
            ms_flags |= ImGuiMultiSelectFlags_SelectOnClickRelease;
        ms_flags |= ImGuiMultiSelectFlags_NavWrapX;

        ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ms_flags, Selection.Size, static_cast<int>(Items.size()));

        Selection.UserData = this;
        Selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self_, int idx)
        {
            AssetsBrowser* self = static_cast<AssetsBrowser*>(self_->UserData);
            return self->Items[idx].ID;
        };
        Selection.ApplyRequests(ms_io);

        const bool want_delete = (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) && (Selection.Size > 0)) ||
            RequestDelete;
        const int item_curr_idx_to_focus = want_delete ? Selection.ApplyDeletionPreLoop(ms_io, static_cast<int>(Items.size())) : -1;
        RequestDelete = false;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(LayoutSelectableSpacing, LayoutSelectableSpacing));

        const ImU32 icon_bg_color = ImGui::GetColorU32(IM_COL32(35, 35, 35, 220));
        const ImVec2 icon_type_overlay_size = ImVec2(4.0f, 4.0f);
        const bool display_label = (LayoutItemSize.x >= ImGui::CalcTextSize("999").x);

        const int column_count = LayoutColumnCount;
        ImGuiListClipper clipper;
        clipper.Begin(LayoutLineCount, LayoutItemStep.y);
        if (item_curr_idx_to_focus != -1)
            clipper.IncludeItemByIndex(item_curr_idx_to_focus / column_count);
        if (ms_io->RangeSrcItem != -1)
            clipper.IncludeItemByIndex(static_cast<int>(ms_io->RangeSrcItem) / column_count);

        while (clipper.Step())
        {
            for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; line_idx++)
            {
                const int item_min_idx_for_current_line = line_idx * column_count;
                const int item_max_idx_for_current_line = NOUS_MathUtils::MIN(
                    (line_idx + 1) * column_count, static_cast<int>(Items.size()));

                for (int item_idx = item_min_idx_for_current_line; item_idx < item_max_idx_for_current_line; ++item_idx)
                {
                    AssetEntry* item_data = &Items[item_idx];
                    ImGui::PushID(static_cast<int>(item_data->ID));

                    ImVec2 pos = ImVec2(start_pos.x + (item_idx % column_count) * LayoutItemStep.x,
                                        start_pos.y + line_idx * LayoutItemStep.y);
                    ImGui::SetCursorScreenPos(pos);

                    ImGui::SetNextItemSelectionUserData(item_idx);
                    bool item_is_selected = Selection.Contains(static_cast<ImGuiID>(item_data->ID));
                    bool item_is_visible  = ImGui::IsRectVisible(LayoutItemSize);

                    if (item_data->fileType == FileType::FOLDER)
                    {
                        ImGui::Selectable("", item_is_selected, ImGuiSelectableFlags_None, LayoutItemSize);

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            directory_stack.push(current_directory);
                            current_directory = current_directory + "/" + item_data->name;
                            AddItemsFromDirectory(current_directory);
                            Selection.Clear();

                            ImGui::PopID();
                            break;
                        }

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEMS"))
                            {
                                const char* data = static_cast<const char*>(payload->Data);
                                const char* end  = data + payload->DataSize;
                                while (data < end)
                                {
                                    std::string srcPath(data);
                                    if (!srcPath.empty())
                                        m_pendingMoves.emplace_back(srcPath, item_data->path);
                                    data += srcPath.size() + 1;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    else
                    {
                        ImGui::Selectable("", item_is_selected, ImGuiSelectableFlags_None, LayoutItemSize);
                    }

                    if (ImGui::IsItemToggledSelection())
                        item_is_selected = !item_is_selected;

                    if (ImGui::BeginItemTooltip())
                    {
                        ImGui::TextUnformatted(item_data->name.c_str());
                        ImGui::EndTooltip();
                    }

                    if (item_curr_idx_to_focus == item_idx)
                        ImGui::SetKeyboardFocusHere(-1);

                    if (ImGui::BeginDragDropSource())
                    {
                        std::string payload;
                        if (!item_is_selected)
                        {
                            payload += item_data->path;
                            payload += '\0';
                        }
                        else
                        {
                            void* it = nullptr;
                            ImGuiID id = 0;
                            while (Selection.GetNextSelectedItem(&it, &id))
                            {
                                if (AssetEntry* sel = GetItemByID(id))
                                {
                                    payload += sel->path;
                                    payload += '\0';
                                }
                            }
                        }

                        ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEMS", payload.data(), payload.size());

                        ImGui::BeginTooltip();
                        const char* p = payload.data();
                        const char* pend = p + payload.size();
                        int count = 0;
                        while (p < pend) { p += strlen(p) + 1; ++count; }
                        ImGui::Text("%d asset(s)", count);
                        ImGui::EndTooltip();

                        ImGui::EndDragDropSource();
                    }

                    if (item_is_visible)
                    {
                        ImVec2 box_min(pos.x - 1, pos.y - 1);
                        ImVec2 box_max(box_min.x + LayoutItemSize.x + 2, box_min.y + LayoutItemSize.y + 2);
                        draw_list->AddRectFilled(box_min, box_max, icon_bg_color);

                        {
                            ImFont* iconFont = editorContext->GetFont(2);
                            if (iconFont)
                            {
                                auto glyphIt = icon_type_glyphs.find(item_data->fileType);
                                const char* glyph = (glyphIt != icon_type_glyphs.end())
                                                        ? glyphIt->second
                                                        : "\xEF\x85\x9B";

                                const ImU32 overlayColor = icon_type_overlay_colors.count(item_data->fileType)
                                                               ? icon_type_overlay_colors.at(item_data->fileType)
                                                               : IM_COL32(204, 204, 204, 255);

                                const float iconFontSize = ImGui::GetFontSize();
                                const float iconScale    = LayoutItemSize.y * 0.55f / iconFontSize;
                                const ImVec2 glyphSize   = iconFont->CalcTextSizeA(
                                    iconFontSize * iconScale, FLT_MAX, 0.0f, glyph);
                                const ImVec2 iconPos = ImVec2(
                                    box_min.x + (LayoutItemSize.x - glyphSize.x) * 0.5f,
                                    box_min.y + (LayoutItemSize.y - glyphSize.y) * 0.5f);
                                draw_list->AddText(iconFont, iconFontSize * iconScale, iconPos, overlayColor, glyph);
                            }
                        }

                        if (ShowTypeOverlay && item_data->fileType != FileType::FOLDER)
                        {
                            ImU32 type_col = icon_type_overlay_colors.at(item_data->fileType);
                            const float overlay_w = icon_type_overlay_size.x * 1.5f;
                            const float overlay_h = icon_type_overlay_size.y * 1.5f;
                            draw_list->AddRectFilled(
                                ImVec2(box_max.x - 2 - overlay_w, box_min.y + 2),
                                ImVec2(box_max.x - 2,             box_min.y + 2 + overlay_h),
                                type_col);
                        }

                        if (display_label)
                        {
                            const float available_width = LayoutItemSize.x + 14;
                            std::string title = item_data->name;
                            ImVec2 text_size = ImGui::CalcTextSize(title.c_str());
                            if (text_size.x > available_width)
                            {
                                int new_length = static_cast<int>(title.size());
                                while (new_length > 0)
                                {
                                    std::string truncated = title.substr(0, new_length);
                                    if (ImGui::CalcTextSize(truncated.c_str()).x <= available_width)
                                    {
                                        title = truncated + "...";
                                        break;
                                    }
                                    --new_length;
                                }
                            }

                            ImU32 label_col = ImGui::GetColorU32(
                                item_is_selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
                            ImVec2 label_pos = ImVec2(pos.x, pos.y + LayoutItemSize.y + 4);
                            ImGui::PushFont(editorContext->GetFont(1));
                            draw_list->AddText(label_pos, label_col, title.c_str());
                            ImGui::PopFont();
                        }
                    }

                    ImGui::PopID();
                }
            }
        }
        clipper.End();

        // An empty directory gives LayoutLineCount == 0, so the clipper runs zero
        // iterations and NOTHING is submitted after the SetCursorScreenPos above --
        // which leaves ImGui's DC.IsSetPos set with CursorPos past CursorMaxPos, and
        // EndChild then trips ErrorCheckUsingSetCursorPosToExtendParentBoundaries.
        // Submitting an item closes that, and an empty folder gets a real empty state
        // instead of a blank panel. (ImGui's own demo browser never has zero items,
        // which is why the case is unhandled upstream.)
        if (Items.empty())
            ImGui::TextDisabled("This folder is empty.");

        // Flush deferred moves (accumulated during folder drop targets above)
        for (const auto& [src, destDir] : m_pendingMoves)
            MoveAsset(src, destDir);
        m_pendingMoves.clear();

        ImGui::PopStyleVar(); // ImGuiStyleVar_ItemSpacing

        if (ImGui::BeginPopupContextWindow())
        {
            ImGui::Text("Selection: %d items", Selection.Size);
            ImGui::Separator();

            if (ImGui::MenuItem("Create Folder"))
            {
                memset(folder_name_buffer, 0, sizeof(folder_name_buffer));
                show_create_folder_popup = true;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Create Script"))
            {
                script_creation_path = current_directory;
                memset(script_name_buffer, 0, sizeof(script_name_buffer));
                show_create_script_popup = true;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Create Material"))
            {
                memset(material_name_buffer, 0, sizeof(material_name_buffer));
                show_create_material_popup = true;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Create Shader"))
            {
                memset(shader_name_buffer, 0, sizeof(shader_name_buffer));
                show_create_shader_popup = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del", false, Selection.Size > 0))
                RequestDelete = true;
            ImGui::EndPopup();
        }

        if (show_create_folder_popup)
        {
            ImGui::OpenPopup("Create New Folder");
            show_create_folder_popup = false;
        }

        if (ImGui::BeginPopupModal("Create New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create folder in: %s", current_directory.c_str());
            ImGui::Spacing();
            ImGui::Text("Folder Name:");
            ImGui::SetNextItemWidth(300.0f);
            bool enter_pressed = ImGui::InputText("##FolderName", folder_name_buffer, IM_ARRAYSIZE(folder_name_buffer),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const bool name_empty = strlen(folder_name_buffer) == 0;
            ImGui::BeginDisabled(name_empty);
            if (ImGui::Button("Create", ImVec2(120, 0)) || (enter_pressed && !name_empty))
            {
                std::filesystem::create_directory(current_directory + "/" + folder_name_buffer);
                AddItemsFromDirectory(current_directory);
                memset(folder_name_buffer, 0, sizeof(folder_name_buffer));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                memset(folder_name_buffer, 0, sizeof(folder_name_buffer));
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (show_create_material_popup)
        {
            ImGui::OpenPopup("Create New Material");
            show_create_material_popup = false;
        }

        if (ImGui::BeginPopupModal("Create New Material", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create material in: %s", current_directory.c_str());
            ImGui::Spacing();
            ImGui::Text("Material Name:");
            ImGui::SetNextItemWidth(300.0f);
            bool enter_pressed = ImGui::InputText("##MaterialName", material_name_buffer, IM_ARRAYSIZE(material_name_buffer),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const bool name_empty = strlen(material_name_buffer) == 0;
            ImGui::BeginDisabled(name_empty);
            if (ImGui::Button("Create", ImVec2(120, 0)) || (enter_pressed && !name_empty))
            {
                const std::string matPath = current_directory + "/" + material_name_buffer + ".nmat";
                constexpr const char* defaultContent =
                    "{\n"
                    "    \"uniforms\": [],\n"
                    "    \"texture_maps\": []\n"
                    "}\n";

                if (std::FILE* f = std::fopen(matPath.c_str(), "w"))
                {
                    std::fputs(defaultContent, f);
                    std::fclose(f);
                    editorContext->GetResourceManager()->ImportFile(matPath);
                    NOUS_INFO("Created material: %s", matPath.c_str());
                    AddItemsFromDirectory(current_directory);
                }
                else
                {
                    NOUS_ERROR("[AssetsBrowser] Failed to create material file: %s", matPath.c_str());
                }

                memset(material_name_buffer, 0, sizeof(material_name_buffer));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                memset(material_name_buffer, 0, sizeof(material_name_buffer));
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (show_create_shader_popup)
        {
            ImGui::OpenPopup("Create New Shader");
            show_create_shader_popup = false;
        }

        if (ImGui::BeginPopupModal("Create New Shader", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create shader in: %s", current_directory.c_str());
            ImGui::Spacing();
            ImGui::Text("Shader Name:");
            ImGui::SetNextItemWidth(300.0f);
            bool enter_pressed = ImGui::InputText("##ShaderName", shader_name_buffer, IM_ARRAYSIZE(shader_name_buffer),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const bool name_empty = strlen(shader_name_buffer) == 0;
            ImGui::BeginDisabled(name_empty);
            if (ImGui::Button("Create", ImVec2(120, 0)) || (enter_pressed && !name_empty))
            {
                const std::string shaderPath = current_directory + "/" + shader_name_buffer + ".glsl";

                if (std::FILE* f = std::fopen(shaderPath.c_str(), "w"))
                {
                    std::fputs(TextEditorWindow::k_DefaultShaderSource, f);
                    std::fclose(f);
                    editorContext->GetResourceManager()->ImportFile(shaderPath);
                    editorContext->WatchShaderFile(shaderPath);
                    NOUS_INFO("Created shader: %s", shaderPath.c_str());
                    AddItemsFromDirectory(current_directory);
                }
                else
                {
                    NOUS_ERROR("[AssetsBrowser] Failed to create shader file: %s", shaderPath.c_str());
                }

                memset(shader_name_buffer, 0, sizeof(shader_name_buffer));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                memset(shader_name_buffer, 0, sizeof(shader_name_buffer));
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (show_create_script_popup)
        {
            ImGui::OpenPopup("Create New Script");
            show_create_script_popup = false;
        }

        if (ImGui::BeginPopupModal("Create New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create a new script in: %s", script_creation_path.c_str());
            ImGui::Spacing();
            ImGui::Text("Script Name:");
            ImGui::SetNextItemWidth(300.0f);
            bool enter_pressed = ImGui::InputText("##ScriptName", script_name_buffer, IM_ARRAYSIZE(script_name_buffer),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const bool name_empty = strlen(script_name_buffer) == 0;
            ImGui::BeginDisabled(name_empty);
            if (ImGui::Button("Create", ImVec2(120, 0)) || (enter_pressed && !name_empty))
            {
                std::string script_name = script_name_buffer;
                if (ScriptManager::GenerateScript(script_name, script_creation_path))
                {
                    NOUS_INFO("Successfully created script: %s", script_name.c_str());
                    AddItemsFromDirectory(current_directory);
                }
                else
                {
                    NOUS_ERROR("Failed to create script: %s", script_name.c_str());
                }
                memset(script_name_buffer, 0, sizeof(script_name_buffer));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                memset(script_name_buffer, 0, sizeof(script_name_buffer));
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ms_io = ImGui::EndMultiSelect();
        Selection.ApplyRequests(ms_io);
        if (want_delete)
        {
            std::vector<std::string> toDelete;
            void* it = nullptr;
            ImGuiID id = 0;
            while (Selection.GetNextSelectedItem(&it, &id))
            {
                if (AssetEntry* item = GetItemByID(id))
                    toDelete.push_back(item->path);
            }
            Selection.Clear();
            for (const auto& path : toDelete)
                DeleteAsset(path);
        }

        if (ImGui::IsWindowAppearing())
            ZoomWheelAccum = 0.0f;
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f && ImGui::IsKeyDown(ImGuiMod_Ctrl) &&
            ImGui::IsAnyItemActive() == false)
        {
            ZoomWheelAccum += io.MouseWheel;
            if (fabsf(ZoomWheelAccum) >= 1.0f)
            {
                const float hovered_item_nx = (io.MousePos.x - start_pos.x + LayoutItemSpacing * 0.5f) / LayoutItemStep.x;
                const float hovered_item_ny = (io.MousePos.y - start_pos.y + LayoutItemSpacing * 0.5f) / LayoutItemStep.y;
                const int hovered_item_idx  = (static_cast<int>(hovered_item_ny) * LayoutColumnCount) +
                                               static_cast<int>(hovered_item_nx);

                IconSize *= powf(1.1f, static_cast<float>(static_cast<int>(ZoomWheelAccum)));
                IconSize = std::clamp(IconSize, 16.0f, 128.0f);
                ZoomWheelAccum -= static_cast<int>(ZoomWheelAccum);
                UpdateLayoutSizes(avail_width);

                float hovered_item_rel_pos_y = (static_cast<float>(hovered_item_idx / LayoutColumnCount) +
                    fmodf(hovered_item_ny, 1.0f)) * LayoutItemStep.y;
                hovered_item_rel_pos_y += ImGui::GetStyle().WindowPadding.y;
                float mouse_local_y = io.MousePos.y - ImGui::GetWindowPos().y;
                ImGui::SetScrollY(hovered_item_rel_pos_y - mouse_local_y);
            }
        }
    }
    ImGui::EndChild();

    ImGui::Text("Selected: %d/%llu items", Selection.Size, static_cast<unsigned long long>(Items.size()));
}

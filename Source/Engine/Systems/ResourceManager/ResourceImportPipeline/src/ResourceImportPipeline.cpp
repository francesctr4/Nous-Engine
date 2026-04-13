#include "Engine/Systems/ResourceManager/ResourceImportPipeline/include/ResourceImportPipeline.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"

#include <filesystem>
#include <format>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

// Returns the effective modification time of a library output path.
// Shader library "paths" are directories containing .spv files — Windows does
// not reliably update a directory's own mtime when its contents change, so we
// scan for the newest regular file inside it instead.
static std::filesystem::file_time_type GetLibraryTime(const std::filesystem::path& libraryPath)
{
    namespace fs = std::filesystem;

    if (fs::is_directory(libraryPath))
    {
        auto newest = fs::file_time_type::min();
        for (const auto& entry : fs::directory_iterator(libraryPath))
            if (fs::is_regular_file(entry))
                newest = std::max(newest, fs::last_write_time(entry));
        return newest;
    }

    return fs::last_write_time(libraryPath);
}

ResourceImportPipeline::ResourceImportPipeline(IImporterManager* importerManager)
    : m_importerManager(importerManager)
{
}

bool ResourceImportPipeline::EnsureLibraryDirectories()
{
    return NOUS_FileManager::CreateDirectory("Library") &&
           NOUS_FileManager::CreateDirectory("Library/Shaders") &&
           NOUS_FileManager::CreateDirectory("Library/Meshes") &&
           NOUS_FileManager::CreateDirectory("Library/Materials") &&
           NOUS_FileManager::CreateDirectory("Library/Textures") &&
           NOUS_FileManager::CreateDirectory("Library/Scenes");
}

void ResourceImportPipeline::ScanAndImportAssets()
{
    // Scan Assets/ on startup. ImportFile is a cheap no-op for assets
    // whose library binary is already up-to-date (Case 3 timestamp check).
    ImportDirectory("Assets");

    // Mirror all scene files from Assets/Scenes/ → Library/Scenes/ so GameApp
    // can always load them from Library/ without needing Assets/.
    if (NOUS_FileManager::Exists("Assets/Scenes"))
    {
        for (const auto& entry : std::filesystem::directory_iterator("Assets/Scenes"))
        {
            if (entry.path().extension() == ".nous")
            {
                const std::string src  = entry.path().string();
                const std::string dest = "Library/Scenes/" + entry.path().filename().string();
                NOUS_FileManager::CopyFile(src, dest);
            }
        }
    }
}

bool ResourceImportPipeline::ImportDirectory(const std::string& directory)
{
    if (!NOUS_FileManager::Exists(directory))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Directory does not exist: %s", directory.c_str());
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (std::filesystem::is_regular_file(entry))
        {
            ImportFile(entry.path().string());
        }
    }

    return true;
}

bool ResourceImportPipeline::ImportFile(const std::string& path)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Importing file: %s", path.c_str());

    if (!NOUS_FileManager::Exists(path))
    {
        NOUS_ERROR("Import File ERROR: General --> Couldn't find file: %s", path.c_str());
        return false;
    }

    const std::string relativePath  = NOUS_FileManager::GetRelativePath(path);
    const std::string fileDirectory = NOUS_FileManager::GetDirectory(path);
    const std::string fileName      = NOUS_FileManager::GetFilename(path);
    const std::string extension     = NOUS_FileManager::GetExtension(path);
    const ResourceType resourceType = Resource::GetTypeFromExtension(extension);

    if (resourceType == ResourceType::UNKNOWN)
        return false;

    if (fileDirectory.rfind("Assets/", 0) == 0)
        return ImportFileFromAssets(relativePath, resourceType, fileName, extension);

    return ImportFileFromExternal(path, resourceType, fileName, extension);
}

bool ResourceImportPipeline::ImportFileFromExternal(const std::string& path, const ResourceType resourceType,
                                                    const std::string& fileName, const std::string& extension)
{
    const std::string newPath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension;
    if (!NOUS_FileManager::CopyFile(path, newPath))
    {
        NOUS_ERROR("Import File ERROR: CASE 0 --> Error while copying the file to Assets\\ directory.");
        return false;
    }
    return ImportFile(newPath);
}

bool ResourceImportPipeline::ImportFileFromAssets(const std::string& relativePath, const ResourceType resourceType,
                                                   const std::string& fileName, const std::string& extension) const
{
    const std::string metaFilePath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension + ".meta";

    if (!NOUS_FileManager::Exists(metaFilePath))
        return ImportCase1_NewAsset(relativePath, metaFilePath, resourceType, fileName);

    MetaFileData metaFileData;
    if (!ReadMetaFile(metaFilePath, metaFileData))
    {
        NOUS_ERROR("Import File ERROR: CASE 2,3 --> Error reading meta file: %s", metaFilePath.c_str());
        return false;
    }

    if (!NOUS_FileManager::Exists(metaFileData.libraryPath))
        return ImportCase2_MissingLibrary(metaFileData);

    return ImportCase3_TimestampCheck(metaFileData);
}

bool ResourceImportPipeline::ImportCase1_NewAsset(const std::string_view relativePath, const std::string& metaFilePath,
                                                   const ResourceType resourceType, const std::string_view fileName) const
{
    const auto resourceUID         = static_cast<uint32>(Random::Generate());
    const std::string libExtension = Resource::GetLibraryExtensionFromType(resourceType);
    std::string libraryPath        = std::format("{}{}", Resource::GetLibraryDirectoryFromType(resourceType), resourceUID);
    if (!libExtension.empty())
        libraryPath += "." + libExtension;

    MetaFileData metaFileData;
    metaFileData.name         = fileName;
    metaFileData.uid          = resourceUID;
    metaFileData.resourceType = resourceType;
    metaFileData.assetsPath   = relativePath;
    metaFileData.libraryPath  = libraryPath;

    if (!CreateMetaFile(metaFilePath, metaFileData))
    {
        NOUS_ERROR("Import File ERROR: CASE 1 --> Error creating meta file: %s", metaFilePath.c_str());
        return false;
    }

    m_importerManager->Import(metaFileData.resourceType, metaFileData);
    return true;
}

bool ResourceImportPipeline::ImportCase2_MissingLibrary(const MetaFileData& metaFileData) const
{
    m_importerManager->Import(metaFileData.resourceType, metaFileData);
    return true;
}

bool ResourceImportPipeline::ImportCase3_TimestampCheck(const MetaFileData& metaFileData) const
{
    if (std::filesystem::last_write_time(metaFileData.assetsPath)
        >
        GetLibraryTime(metaFileData.libraryPath))
    {
        NOUS_INFO_C(CURRENT_CHANNEL,
            "[ImportFile] '%s' modified since last import — regenerating library binary.",
            metaFileData.name.c_str());

        m_importerManager->Import(metaFileData.resourceType, metaFileData);
    }

    return true;
}

bool ResourceImportPipeline::CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData)
{
    JsonFile metaFile;

    metaFile.AppendValue("Name", inFileData.name);
    metaFile.AppendValue("UID", static_cast<double>(inFileData.uid));
    metaFile.AppendValue("Resource Type", std::to_underlying(inFileData.resourceType));
    metaFile.AppendValue("Assets Path", inFileData.assetsPath);
    metaFile.AppendValue("Library Path", inFileData.libraryPath);

    return metaFile.SaveToFile(metaFilePath.c_str());
}

bool ResourceImportPipeline::ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData)
{
    JsonFile metaFile;

    if (!metaFile.LoadFromFile(metaFilePath.c_str()))
        return false;

    std::string r_fileName;
    std::string r_assetsPath;
    std::string r_libraryPath;
    int         r_resourceType;
    double      r_resourceUID;

    if (!metaFile.GetValue("Name", r_fileName) ||
        !metaFile.GetValue("UID", r_resourceUID) ||
        !metaFile.GetValue("Resource Type", r_resourceType) ||
        !metaFile.GetValue("Assets Path", r_assetsPath) ||
        !metaFile.GetValue("Library Path", r_libraryPath))
    {
        return false;
    }

    outFileData.name         = r_fileName;
    outFileData.uid          = static_cast<uint32>(r_resourceUID);
    outFileData.resourceType = static_cast<ResourceType>(r_resourceType);
    outFileData.assetsPath   = r_assetsPath;
    outFileData.libraryPath  = r_libraryPath;

    return true;
}

bool ResourceImportPipeline::GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData)
{
    return ReadMetaFile(assetsPath + ".meta", outData);
}

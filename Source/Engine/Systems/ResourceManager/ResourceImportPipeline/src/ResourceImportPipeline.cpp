#include "Engine/Systems/ResourceManager/ResourceImportPipeline/include/ResourceImportPipeline.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
#include "Engine/Systems/ResourceManager/ResourceTypeRegistry/ResourceTypeRegistry.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <filesystem>
#include <format>
#include <latch>
#include <vector>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

// Builds the library output path for a freshly assigned UID, honoring each
// type's LibraryExtPolicy. The source extension argument is only consulted
// when the policy is PRESERVE_SOURCE (e.g. AUDIO keeps .wav/.ogg) and may
// include a leading dot.
static std::string BuildLibraryFilename(const ResourceTypeDescriptor& d,
                                        uint32 uid,
                                        const std::string& sourceExtensionWithDot)
{
    std::string out = std::format("{}{}", d.libraryFolder, uid);
    switch (d.libExtPolicy)
    {
        case LibraryExtPolicy::FIXED:
            if (!d.libraryFixedExtension.empty())
                out += "." + d.libraryFixedExtension;
            break;
        case LibraryExtPolicy::PRESERVE_SOURCE:
            out += sourceExtensionWithDot; // already includes the leading dot
            break;
        case LibraryExtPolicy::DIRECTORY_OF_STAGES:
            // No extension: the library "path" is a directory.
            break;
    }
    return out;
}

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

ResourceImportPipeline::ResourceImportPipeline(IImporterManager* importerManager,
                                               nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : m_importerManager(importerManager)
    , m_jobSystem(jobSystem)
{
}

// Descriptors store libraryFolder with a trailing slash for path concatenation
// ({folder}{uid}). Filesystem ops want the canonical dir name without it.
static std::string StripTrailingSlash(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    return path;
}

void ResourceImportPipeline::ClearLibraryFiles()
{
    namespace fs = std::filesystem;

    for (const ResourceTypeDescriptor* d : GetResourceTypeRegistry().All())
    {
        std::error_code ec;
        fs::remove_all(StripTrailingSlash(d->libraryFolder), ec);
    }
    // Scenes are not a registered ResourceType (mirrored separately by ScanAndImportAssets).
    {
        std::error_code ec;
        fs::remove_all("Library/Scenes", ec);
    }

    EnsureLibraryDirectories();
}

bool ResourceImportPipeline::EnsureLibraryDirectories()
{
    if (!nous::engine::filesystem::CreateDirectory("Library"))
        return false;

    for (const ResourceTypeDescriptor* d : GetResourceTypeRegistry().All())
    {
        if (!nous::engine::filesystem::CreateDirectory(StripTrailingSlash(d->libraryFolder)))
            return false;
    }
    // Scenes — not a registered ResourceType, but uses the Library tree.
    return nous::engine::filesystem::CreateDirectory("Library/Scenes");
}

void ResourceImportPipeline::ScanAndImportAssets(const bool parallelImports)
{
    // Phase 1 (sequential): walk Assets/, write any missing .meta files, collect
    // MetaFileData for every file that actually needs import work.
    std::vector<MetaFileData> pendingWork;
    CollectPendingImports("Assets", pendingWork);

    // Phase 2: import in parallel when a job system is available, otherwise sequential.
    // parallelImports is false when called from a job-system worker with no other free
    // workers — submitting sub-jobs and waiting on a latch would starve the queue.
    if (m_jobSystem && parallelImports && !pendingWork.empty())
    {
        // Split mesh imports from everything else.
        // Assimp is CPU/memory-intensive: running many mesh jobs concurrently
        // causes cache thrashing and is slower than sequential processing.
        // Shaders (~7s peak) and other light assets run in parallel on all threads;
        // all meshes run sequentially in one job — total is max(shaders, meshes)
        // rather than max(each mesh * contention_factor).
        std::vector<MetaFileData> meshWork;
        int jobCount = 0;
        for (const auto& item : pendingWork)
        {
            if (item.resourceType == ResourceType::MESH)
                meshWork.push_back(item);
            else
                ++jobCount;
        }
        if (!meshWork.empty())
            ++jobCount;

        // Use a latch instead of WaitForPendingJobs() so this function is safe to
        // call from a job-system worker without deadlocking on the shared pending counter.
        std::latch doneLatch{ jobCount };

        for (const auto& item : pendingWork)
        {
            if (item.resourceType == ResourceType::MESH)
                continue;
            m_jobSystem->SubmitJob([this, item, &doneLatch]()
            {
                m_importerManager->Import(item.resourceType, item);
                doneLatch.count_down();
            }, item.name);
        }

        if (!meshWork.empty())
        {
            m_jobSystem->SubmitJob([this, meshWork = std::move(meshWork), &doneLatch]()
            {
                for (const auto& mesh : meshWork)
                    m_importerManager->Import(mesh.resourceType, mesh);
                doneLatch.count_down();
            }, "MeshImports");
        }

        doneLatch.wait();
    }
    else
    {
        for (const auto& item : pendingWork)
            m_importerManager->Import(item.resourceType, item);
    }

    // Mirror ALL .nous scene files found anywhere inside Assets/ → Library/Scenes/.
    // Library paths use the filename only; if two scenes share a name in different
    // subdirectories the last one written wins — users should use unique scene names.
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator("Assets", ec))
        {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".nous") continue;

            const std::string src  = entry.path().generic_string();
            const std::string dest = "Library/Scenes/" + entry.path().filename().string();
            nous::engine::filesystem::CopyFile(src, dest);
        }
    }

    WriteShaderManifest();
}

void ResourceImportPipeline::WriteShaderManifest()
{
    // Built-in shaders that GAME mode loads via the manifest (no .meta read at runtime).
    // Asset paths mirror the CreateResource calls in ModuleRenderer3D::Start.
    struct Entry { const char* key; const char* assetsPath; };
    static constexpr Entry c_builtIns[] = {
        { "MaterialShader",   "Assets/Shaders/BuiltIn.MaterialShader.glsl"   },
        { "BackgroundShader", "Assets/Shaders/BuiltIn.BackgroundShader.glsl" },
    };

    JsonObject root;
    for (const auto& [key, assetsPath] : c_builtIns)
    {
        MetaFileData meta;
        if (!GetAssetMetaData(assetsPath, meta))
        {
            NOUS_WARN_C(CURRENT_CHANNEL,
                "shader_manifest.json: missing .meta for built-in shader '%s' — manifest not written.",
                assetsPath);
            return;
        }
        JsonObject entry;
        entry.Set("uid",         static_cast<double>(meta.uid));
        entry.Set("libraryPath", meta.libraryPath);
        root.Set(key, std::move(entry));
    }

    if (!JsonFile::SaveToFile(root, "Library/Shaders/shader_manifest.json"))
        NOUS_WARN_C(CURRENT_CHANNEL, "Failed to write Library/Shaders/shader_manifest.json.");
    else
        NOUS_INFO_C(CURRENT_CHANNEL, "Shader manifest written to Library/Shaders/shader_manifest.json.");
}

void ResourceImportPipeline::CollectPendingImports(const std::string& directory,
                                                    std::vector<MetaFileData>& outPending) const
{
    if (!nous::engine::filesystem::Exists(directory))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Directory does not exist: %s", directory.c_str());
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (!std::filesystem::is_regular_file(entry))
            continue;

        const std::string path          = entry.path().string();
        const std::string relativePath  = nous::engine::filesystem::GetRelativePath(path);
        const std::string fileDirectory = nous::engine::filesystem::GetDirectory(path);
        const std::string fileName      = nous::engine::filesystem::GetFilename(path);
        const std::string extension     = nous::engine::filesystem::GetExtension(path);
        const ResourceType resourceType = Resource::GetTypeFromExtension(extension);

        if (resourceType == ResourceType::UNKNOWN)
            continue;
        if (fileDirectory.rfind("Assets/", 0) != 0)
            continue;

        NOUS_DEBUG_C(CURRENT_CHANNEL, "Importing file: %s", path.c_str());

        const std::string metaFilePath = fileDirectory + fileName + extension + ".meta";

        if (!nous::engine::filesystem::Exists(metaFilePath))
        {
            // Case 1: new asset — create meta file now (sequential), schedule import.
            const auto resourceUID = static_cast<uint32>(Random::Generate());
            const ResourceTypeDescriptor* desc = GetResourceTypeRegistry().Get(resourceType);
            if (!desc)
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "Import File ERROR: no registry descriptor for resource type of '%s'", path.c_str());
                continue;
            }
            const std::string libraryPath = BuildLibraryFilename(*desc, resourceUID, extension);

            MetaFileData meta;
            meta.name         = fileName;
            meta.uid          = resourceUID;
            meta.resourceType = resourceType;
            meta.assetsPath   = relativePath;
            meta.libraryPath  = libraryPath;

            if (CreateMetaFile(metaFilePath, meta))
                outPending.push_back(meta);
            else
                NOUS_ERROR_C(CURRENT_CHANNEL, "Import File ERROR: CASE 1 --> Error creating meta file: %s", metaFilePath.c_str());
        }
        else
        {
            MetaFileData meta;
            if (!ReadMetaFile(metaFilePath, meta))
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "Import File ERROR: CASE 2,3 --> Error reading meta file: %s", metaFilePath.c_str());
                continue;
            }

            if (!nous::engine::filesystem::Exists(meta.libraryPath))
            {
                // Case 2: library binary missing.
                outPending.push_back(meta);
            }
            else if (std::filesystem::last_write_time(meta.assetsPath) > GetLibraryTime(meta.libraryPath))
            {
                // Case 3: source file modified since last import.
                NOUS_INFO_C(CURRENT_CHANNEL,
                    "[ImportFile] '%s' modified since last import — regenerating library binary.",
                    meta.name.c_str());
                outPending.push_back(meta);
            }
        }
    }
}

bool ResourceImportPipeline::ImportDirectory(const std::string& directory)
{
    if (!nous::engine::filesystem::Exists(directory))
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

    if (!nous::engine::filesystem::Exists(path))
    {
        NOUS_ERROR("Import File ERROR: General --> Couldn't find file: %s", path.c_str());
        return false;
    }

    const std::string relativePath  = nous::engine::filesystem::GetRelativePath(path);
    const std::string fileDirectory = nous::engine::filesystem::GetDirectory(path);
    const std::string fileName      = nous::engine::filesystem::GetFilename(path);
    const std::string extension     = nous::engine::filesystem::GetExtension(path);
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
    const std::string newPath = "Assets/" + fileName + extension;
    if (!nous::engine::filesystem::CopyFile(path, newPath))
    {
        NOUS_ERROR("Import File ERROR: CASE 0 --> Error while copying the file to Assets\\ directory.");
        return false;
    }
    return ImportFile(newPath);
}

bool ResourceImportPipeline::ImportFileFromAssets(const std::string& relativePath, const ResourceType resourceType,
                                                   const std::string& fileName, const std::string& extension) const
{
    const std::string fileDirectory = nous::engine::filesystem::GetDirectory(relativePath);
    const std::string metaFilePath  = fileDirectory + fileName + extension + ".meta";

    if (!nous::engine::filesystem::Exists(metaFilePath))
        return ImportCase1_NewAsset(relativePath, metaFilePath, resourceType, fileName);

    MetaFileData metaFileData;
    if (!ReadMetaFile(metaFilePath, metaFileData))
    {
        NOUS_ERROR("Import File ERROR: CASE 2,3 --> Error reading meta file: %s", metaFilePath.c_str());
        return false;
    }

    if (!nous::engine::filesystem::Exists(metaFileData.libraryPath))
        return ImportCase2_MissingLibrary(metaFileData);

    return ImportCase3_TimestampCheck(metaFileData);
}

bool ResourceImportPipeline::ImportCase1_NewAsset(const std::string_view relativePath, const std::string& metaFilePath,
                                                   const ResourceType resourceType, const std::string_view fileName) const
{
    const auto resourceUID = static_cast<uint32>(Random::Generate());
    const ResourceTypeDescriptor* desc = GetResourceTypeRegistry().Get(resourceType);
    if (!desc)
    {
        NOUS_ERROR("Import File ERROR: CASE 1 --> no registry descriptor for type");
        return false;
    }
    const std::string libraryPath = BuildLibraryFilename(*desc, resourceUID,
        nous::engine::filesystem::GetExtension(std::string(relativePath)));

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
    JsonObject metaObj;
    metaObj.Set("Name",          inFileData.name);
    metaObj.Set("UID",           static_cast<double>(inFileData.uid));
    metaObj.Set("Resource Type", std::to_underlying(inFileData.resourceType));
    metaObj.Set("Assets Path",   inFileData.assetsPath);
    metaObj.Set("Library Path",  inFileData.libraryPath);
    return JsonFile::SaveToFile(metaObj, metaFilePath);
}

bool ResourceImportPipeline::ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData)
{
    JsonObject metaObj = JsonFile::LoadFromFile(metaFilePath);
    if (metaObj.IsEmpty())
        return false;

    if (!metaObj.HasKey("Name") ||
        !metaObj.HasKey("UID") ||
        !metaObj.HasKey("Resource Type") ||
        !metaObj.HasKey("Assets Path") ||
        !metaObj.HasKey("Library Path"))
    {
        return false;
    }

    outFileData.name         = metaObj.GetString("Name");
    outFileData.uid          = static_cast<uint32>(metaObj.GetDouble("UID"));
    outFileData.resourceType = static_cast<ResourceType>(metaObj.GetInt("Resource Type"));
    outFileData.assetsPath   = metaObj.GetString("Assets Path");
    outFileData.libraryPath  = metaObj.GetString("Library Path");

    return true;
}

bool ResourceImportPipeline::GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData)
{
    return ReadMetaFile(assetsPath + ".meta", outData);
}

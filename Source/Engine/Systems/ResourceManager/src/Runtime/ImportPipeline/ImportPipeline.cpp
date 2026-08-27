#include <ResourceManager/Runtime/ImportPipeline.h>

#include <FileSystem/FileSystem.h>
#include <Logger/Logger.h>
#include <ResourceManager/Core/AssetPaths.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Core/IImporterDispatcher.h>
#include <ResourceManager/Core/TypeRegistry.h>
#include <ResourceManager/Core/TypeRegistry.h>
#include <VideoSystem/AudioExtract/AudioExtract.h>
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include <NOUS_Multithreading/NOUS_JobSystem.h>

#include <filesystem>
#include <format>
#include <latch>
#include <vector>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

// Builds the library output path for a freshly assigned UID, honoring each
// type's LibraryExtPolicy. The source extension argument is only consulted
// when the policy is PRESERVE_SOURCE (e.g. AUDIO keeps .wav/.ogg) and may
// include a leading dot.
static std::string BuildLibraryFilename(const TypeDescriptor& d,
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

ImportPipeline::ImportPipeline(IImporterDispatcher* importerManager,
                                               const TypeRegistry& typeRegistry,
                                               nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : m_importerManager(importerManager)
    , m_typeRegistry(&typeRegistry)
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

void ImportPipeline::ClearLibraryFiles()
{
    namespace fs = std::filesystem;

    for (const TypeDescriptor* d : m_typeRegistry->All())
    {
        std::error_code ec;
        fs::remove_all(StripTrailingSlash(d->libraryFolder), ec);
    }

    EnsureLibraryDirectories();
}

bool ImportPipeline::EnsureLibraryDirectories()
{
    namespace ap = nous::engine::asset_paths;
    if (!nous::engine::filesystem::CreateDirectory(std::string(ap::k_LibraryDir)))
        return false;

    for (const TypeDescriptor* d : m_typeRegistry->All())
    {
        if (!nous::engine::filesystem::CreateDirectory(StripTrailingSlash(d->libraryFolder)))
            return false;
    }
    return true;
}

void ImportPipeline::ScanAndImportAssets(const bool parallelImports)
{
    // Phase 1 (sequential): walk Assets/, write any missing .meta files, collect
    // MetaFileData for every file that actually needs import work.
    std::vector<MetaFileData> pendingWork;
    CollectPendingImports(std::string(nous::engine::asset_paths::k_AssetsDir), pendingWork);

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

    // Scenes are now imported generically by the per-type pipeline above:
    // CollectPendingImports picks up .nous as ResourceType::SCENE, writes its
    // .meta, and ImporterScene mirrors the source into Library/Scenes/<uid>.nous.

    // Import companion .ogg files produced by video extraction this pass. A video's
    // audio is extracted into Assets/<name>.ogg during its Phase-2 Import, so it was
    // not present during the Phase-1 scan above — import it now so its ResourceAudio
    // exists in the same session (the auto-paired CAudioSource resolves it).
    for (const auto& item : pendingWork)
    {
        if (item.resourceType != ResourceType::VIDEO)
            continue;
        const std::string oggPath = MakeCompanionOggPath(item.assetsPath);
        if (nous::engine::filesystem::Exists(oggPath))
            ImportFile(oggPath);
    }

    WriteShaderManifest();
    WriteSceneManifest();
}

void ImportPipeline::WriteShaderManifest()
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

void ImportPipeline::WriteSceneManifest()
{
    namespace fs = std::filesystem;
    namespace ap = nous::engine::asset_paths;

    JsonObject root;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(ap::k_AssetsDir, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ap::k_SceneExtension) continue;

        // The .meta sits next to the source asset; GetAssetMetaData appends
        // ".meta" to the asset path, matching how CollectPendingImports wrote it.
        const std::string assetsPath = entry.path().string();

        MetaFileData meta;
        if (!GetAssetMetaData(assetsPath, meta))
        {
            NOUS_WARN_C(CURRENT_CHANNEL,
                "scene_manifest.json: missing/invalid .meta for scene '%s' — skipped.",
                assetsPath.c_str());
            continue;
        }

        // Keyed by scene name (filename without extension). Two scenes sharing a
        // name in different subdirectories collide — last one written wins, same
        // caveat the old filename-keyed mirror had. Use unique scene names.
        JsonObject sceneEntry;
        sceneEntry.Set("uid",         static_cast<double>(meta.uid));
        sceneEntry.Set("libraryPath", meta.libraryPath);
        root.Set(meta.name, std::move(sceneEntry));
    }

    const std::string manifestPath = std::string(ap::k_ScenesDir) + "/scene_manifest.json";
    if (!JsonFile::SaveToFile(root, manifestPath))
        NOUS_WARN_C(CURRENT_CHANNEL, "Failed to write %s.", manifestPath.c_str());
    else
        NOUS_INFO_C(CURRENT_CHANNEL, "Scene manifest written to %s.", manifestPath.c_str());
}

std::string ImportPipeline::ResolveSceneLibraryPath(const std::string& sceneName)
{
    namespace ap = nous::engine::asset_paths;

    const std::string manifestPath = std::string(ap::k_ScenesDir) + "/scene_manifest.json";
    JsonObject root = JsonFile::LoadFromFile(manifestPath);
    if (root.IsEmpty() || !root.HasKey(sceneName))
        return {};

    return root.GetObject(sceneName).GetString("libraryPath");
}

std::vector<std::string> ImportPipeline::GetSceneNames()
{
    namespace ap = nous::engine::asset_paths;

    const std::string manifestPath = std::string(ap::k_ScenesDir) + "/scene_manifest.json";
    JsonObject root = JsonFile::LoadFromFile(manifestPath);
    if (root.IsEmpty())
        return {};

    return root.GetKeys();
}

void ImportPipeline::CollectPendingImports(const std::string& directory,
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
        const ResourceType resourceType = m_typeRegistry->TypeFromExtension(extension);

        if (resourceType == ResourceType::UNKNOWN)
            continue;
        if (!nous::engine::asset_paths::IsAssetsRelative(fileDirectory))
            continue;

        NOUS_DEBUG_C(CURRENT_CHANNEL, "Importing file: %s", path.c_str());

        const std::string metaFilePath = nous::engine::asset_paths::MakeMetaFilePath(fileDirectory + fileName + extension);

        if (!nous::engine::filesystem::Exists(metaFilePath))
        {
            // Case 1: new asset — create meta file now (sequential), schedule import.
            const auto resourceUID = static_cast<uint32>(Random::Generate());
            const TypeDescriptor* desc = m_typeRegistry->Get(resourceType);
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

bool ImportPipeline::ImportDirectory(const std::string& directory)
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

bool ImportPipeline::ImportFile(const std::string& path)
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
    const ResourceType resourceType = m_typeRegistry->TypeFromExtension(extension);

    if (resourceType == ResourceType::UNKNOWN)
        return false;

    bool ok;
    if (nous::engine::asset_paths::IsAssetsRelative(fileDirectory))
        ok = ImportFileFromAssets(relativePath, resourceType, fileName, extension);
    else
        ok = ImportFileFromExternal(path, resourceType, fileName, extension);

    // After a video import, import its extracted companion .ogg so the audio
    // ResourceAudio is available immediately (e.g. a video dropped via the editor).
    // The .ogg import runs ImporterAudio, which never writes back into Assets/, so
    // this cannot re-enter the video import.
    if (ok && resourceType == ResourceType::VIDEO)
    {
        const std::string oggPath = MakeCompanionOggPath(relativePath);
        if (nous::engine::filesystem::Exists(oggPath))
            ImportFile(oggPath);
    }
    return ok;
}

bool ImportPipeline::ImportFileFromExternal(const std::string& path, const ResourceType resourceType,
                                                    const std::string& fileName, const std::string& extension)
{
    const std::string newPath = std::string(nous::engine::asset_paths::k_AssetsPrefix) + fileName + extension;
    if (!nous::engine::filesystem::CopyFile(path, newPath))
    {
        NOUS_ERROR("Import File ERROR: CASE 0 --> Error while copying the file to Assets\\ directory.");
        return false;
    }
    return ImportFile(newPath);
}

bool ImportPipeline::ImportFileFromAssets(const std::string& relativePath, const ResourceType resourceType,
                                                   const std::string& fileName, const std::string& extension) const
{
    const std::string fileDirectory = nous::engine::filesystem::GetDirectory(relativePath);
    const std::string metaFilePath  = nous::engine::asset_paths::MakeMetaFilePath(fileDirectory + fileName + extension);

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

bool ImportPipeline::ImportCase1_NewAsset(const std::string_view relativePath, const std::string& metaFilePath,
                                                   const ResourceType resourceType, const std::string_view fileName) const
{
    const auto resourceUID = static_cast<uint32>(Random::Generate());
    const TypeDescriptor* desc = m_typeRegistry->Get(resourceType);
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

bool ImportPipeline::ImportCase2_MissingLibrary(const MetaFileData& metaFileData) const
{
    m_importerManager->Import(metaFileData.resourceType, metaFileData);
    return true;
}

bool ImportPipeline::ImportCase3_TimestampCheck(const MetaFileData& metaFileData) const
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

bool ImportPipeline::CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData)
{
    JsonObject metaObj;
    metaObj.Set("Name",          inFileData.name);
    metaObj.Set("UID",           static_cast<double>(inFileData.uid));
    metaObj.Set("Resource Type", std::to_underlying(inFileData.resourceType));
    metaObj.Set("Assets Path",   inFileData.assetsPath);
    metaObj.Set("Library Path",  inFileData.libraryPath);
    return JsonFile::SaveToFile(metaObj, metaFilePath);
}

bool ImportPipeline::ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData)
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
    // Normalize separators: .meta files authored on Windows store backslash paths
    // (e.g. "Assets\Shaders\x.glsl"). On POSIX a backslash is a literal filename
    // character, so such a path resolves to nothing. NormalizePath makes the stored
    // paths platform-independent on read.
    outFileData.assetsPath   = nous::engine::filesystem::NormalizePath(metaObj.GetString("Assets Path"));
    outFileData.libraryPath  = nous::engine::filesystem::NormalizePath(metaObj.GetString("Library Path"));

    return true;
}

bool ImportPipeline::GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData)
{
    return ReadMetaFile(nous::engine::asset_paths::MakeMetaFilePath(assetsPath), outData);
}

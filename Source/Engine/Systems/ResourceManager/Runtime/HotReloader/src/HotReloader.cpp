#include "Engine/Systems/ResourceManager/Runtime/HotReloader/include/HotReloader.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporterManager.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/MetaFileData.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"
#include "Engine/Systems/ResourceManager/Runtime/ImportPipeline/include/ImportPipeline.h"

#include <algorithm>
#include <filesystem>
#include <thread>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

namespace
{
    // Watcher ticks once every k_PollIntervalFrames PreUpdate calls — full
    // per-frame stat-ing of every tracked asset would be wasteful, and the
    // 30-frame cadence (~0.5s at 60 FPS) is well within human reaction time
    // for "I saved the file and want to see it reload".
    constexpr uint32_t k_PollIntervalFrames = 30;
}

/*static*/ std::string HotReloader::NormalizeAssetPath(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

/*static*/ std::string HotReloader::FindSourceAssetsDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    if (ec) return {};

    for (int depth = 0; depth < 8; ++depth)
    {
        if (fs::exists(dir / "Assets", ec) && fs::exists(dir / "Source", ec))
            return NormalizeAssetPath((dir / "Assets").generic_string());
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = std::move(parent);
    }
    return {};
}

/*static*/ bool HotReloader::IsHotReloadableExtension(const std::string& extWithDot)
{
    if (extWithDot.size() < 2 || extWithDot[0] != '.') return false;
    const std::string_view ext(extWithDot.data() + 1, extWithDot.size() - 1);

    for (const TypeDescriptor* d : GetTypeRegistry().All())
    {
        if (!d || !d->hotReloadable) continue;
        for (const auto& src : d->sourceExtensions)
            if (src == ext) return true;
    }
    return false;
}

HotReloader::HotReloader(IImporterManager* importerManager,
                         nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : m_importerManager(importerManager)
    , m_jobSystem(jobSystem)
{
}

void HotReloader::Disable() noexcept
{
    m_enabled = false;
}

bool HotReloader::IsEnabled() const noexcept
{
    return m_enabled;
}

void HotReloader::Start()
{
    if (!m_enabled) return;
    m_sourceAssetsDir = FindSourceAssetsDir();
    ScanAndRegisterWatchedAssets();
}

void HotReloader::Poll()
{
    if (!m_enabled) return;

    // Drain any watch registrations queued from worker threads (scene preload)
    // or from main-thread TrackAsset calls.
    std::vector<std::string> pending;
    {
        std::scoped_lock lock(m_pendingRegMutex);
        std::swap(pending, m_pendingRegistrations);
    }
    for (const auto& assetsPath : pending)
        RegisterWatchForAsset(assetsPath);

    if ((++m_pollFrameCounter % k_PollIntervalFrames) == 0)
        m_watcher.Poll();
}

void HotReloader::WaitForInFlight()
{
    // Worker lambdas capture `this` and write into m_ready — letting them run
    // after the reloader (or its owning module) is destroyed would be a UAF.
    while (m_inFlightJobCount.load(std::memory_order_acquire) > 0)
        std::this_thread::yield();
}

void HotReloader::TrackAsset(const std::string& assetsPath, uint32 uid, ResourceType type)
{
    if (!m_enabled || assetsPath.empty()) return;
    // Hot-reload eligibility is owned by the TypeDescriptor — adding a new
    // hot-reloadable type is a one-line flag flip in RegisterResourceTypes.
    const TypeDescriptor* d = GetTypeRegistry().Get(type);
    if (!d || !d->hotReloadable) return;

    const std::string key = NormalizeAssetPath(assetsPath);
    {
        std::scoped_lock lock(m_trackedMutex);
        m_tracked[key] = TrackedAsset{ uid, type, key };
    }
    // FileWatcher is single-threaded; defer registration to the main thread.
    std::scoped_lock lock(m_pendingRegMutex);
    m_pendingRegistrations.push_back(assetsPath);
}

void HotReloader::UntrackAsset(const std::string& assetsPath)
{
    if (!m_enabled || assetsPath.empty()) return;
    std::scoped_lock lock(m_trackedMutex);
    m_tracked.erase(NormalizeAssetPath(assetsPath));
}

void HotReloader::RegisterWatchForAsset(const std::string& assetsPath)
{
    if (!m_enabled || m_sourceAssetsDir.empty() || assetsPath.empty())
        return;

    const auto extPos = assetsPath.find_last_of('.');
    if (extPos == std::string::npos) return;
    if (!IsHotReloadableExtension(assetsPath.substr(extPos))) return;

    const std::string relKey = NormalizeAssetPath(assetsPath);

    // Map "Assets/foo/bar.png" -> "<sourceAssetsDir>/foo/bar.png".
    // m_sourceAssetsDir already ends in "/Assets" (no trailing slash).
    std::string watchPath;
    if (relKey.size() > 7 && relKey.substr(0, 7) == "Assets/")
        watchPath = m_sourceAssetsDir + "/" + relKey.substr(7);
    else
        watchPath = relKey;

    m_watcher.WatchIfNew(watchPath, [this, relKey](const std::string&)
    {
        NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] FileWatcher fired for: %s", relKey.c_str());
        DispatchReimportJob(relKey);
    });
}

void HotReloader::ScanAndRegisterWatchedAssets()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    int watchCount = 0;

    const std::string watchBase = m_sourceAssetsDir.empty() ? "Assets" : m_sourceAssetsDir;
    const fs::path sourceRoot = m_sourceAssetsDir.empty()
        ? fs::path{}
        : fs::path(m_sourceAssetsDir).parent_path();

    for (auto& entry : fs::recursive_directory_iterator(watchBase, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (!IsHotReloadableExtension(entry.path().extension().string())) continue;

        const std::string watchPath = NormalizeAssetPath(entry.path().generic_string());

        std::string relKey;
        if (!m_sourceAssetsDir.empty())
            relKey = NormalizeAssetPath(fs::relative(entry.path(), sourceRoot, ec).generic_string());
        else
            relKey = watchPath;

        // WatchIfNew (not Watch) so a periodic rescan does not reset the
        // baseline mtime of a file that already has a pending modification.
        m_watcher.WatchIfNew(watchPath, [this, relKey](const std::string&)
        {
            NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] FileWatcher fired for: %s", relKey.c_str());
            DispatchReimportJob(relKey);
        });
        ++watchCount;
    }

    if (ec)
        NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] Could not scan for asset watching: %s", ec.message().c_str());
}

void HotReloader::DispatchReimportJob(const std::string& normalizedPath)
{
    NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] DispatchReimportJob called for: %s", normalizedPath.c_str());

    uint32       uid  = 0;
    ResourceType type = ResourceType::UNKNOWN;
    std::string  assetsPath;
    {
        std::scoped_lock lock(m_trackedMutex);
        const auto it = m_tracked.find(normalizedPath);
        if (it == m_tracked.end())
        {
            NOUS_WARN_C(CURRENT_CHANNEL,
                "[AssetHotReload] '%s' not tracked (%zu entries known).",
                normalizedPath.c_str(), m_tracked.size());
            return;
        }
        uid        = it->second.uid;
        type       = it->second.type;
        assetsPath = it->second.assetsPath;
    }

    // Dedup: at most one in-flight reimport per UID.
    {
        std::scoped_lock lock(m_inFlightMutex);
        if (!m_inFlightUIDs.insert(uid).second) return;
    }
    ++m_inFlightJobCount;

    if (!m_jobSystem)
    {
        { std::scoped_lock lock(m_inFlightMutex); m_inFlightUIDs.erase(uid); }
        --m_inFlightJobCount;
        return;
    }

    // Capture source dir so the worker can redirect Import to the source file.
    const std::string sourceAssetsDir = m_sourceAssetsDir;

    m_jobSystem->SubmitJob([this, uid, type, assetsPath, sourceAssetsDir]
    {
        auto cleanup = [this, uid]
        {
            { std::scoped_lock lock(m_inFlightMutex); m_inFlightUIDs.erase(uid); }
            --m_inFlightJobCount;
        };

        // 1. Read .meta sidecar to get confirmed library path / type.
        MetaFileData metaData;
        if (!ImportPipeline::GetAssetMetaData(assetsPath, metaData))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] No .meta found for '%s' — skipping reimport.", assetsPath.c_str());
            cleanup(); return;
        }

        // Redirect Import to the source file so it reads what the user actually
        // edited, not the stale configure-time binary copy (e.g. Build/bin/Assets/...).
        // assetsPath is like "Assets/foo/bar.nmat"; sourceAssetsDir ends with "/Assets".
        if (!sourceAssetsDir.empty() && assetsPath.size() > 7
            && assetsPath.substr(0, 7) == "Assets/")
        {
            metaData.assetsPath = sourceAssetsDir + "/" + assetsPath.substr(7);
        }

        // 2. Reimport source -> Library binary (CPU only, no Vulkan).
        if (!m_importerManager->Import(type, metaData))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] Import failed for '%s' — old asset kept.", assetsPath.c_str());
            cleanup(); return;
        }

        // 3. Hand off to the main thread for Deserialize + GPU Release + Upload.
        // Deserialize must NOT run here: it mutates live resource fields (e.g.
        // poolOwnerShader via SetShader) that the main thread reads concurrently
        // in DrawGeometryBatched, causing a crash.
        { std::scoped_lock lock(m_readyMutex); m_ready.push_back({ uid, type }); }
        NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] Queued GPU upload for '%s'.", assetsPath.c_str());

        cleanup();
    }, "ReimportAsset");
}

std::vector<HotReloader::ReadyUpload> HotReloader::TakeReadyUploads()
{
    std::vector<ReadyUpload> result;
    std::scoped_lock lock(m_readyMutex);
    std::swap(result, m_ready);
    return result;
}

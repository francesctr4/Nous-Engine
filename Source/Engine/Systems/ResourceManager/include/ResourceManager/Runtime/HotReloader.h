#pragma once

#include <FileWatcher/FileWatcher.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceUpload.h>
#include <ResourceManager/Types/ResourceType.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

class IImporterDispatcher;
class TypeRegistry;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

// Owns the asset hot-reload pipeline previously embedded in ModuleResourceManager:
//  - the FileWatcher and its initial scan
//  - the path -> (uid, type, assetsPath) tracking map
//  - the in-flight UID dedup set and the worker-thread job counter
//  - the worker -> main-thread "ready upload" handoff queue
//  - the deferred watch-registration queue (worker threads cannot touch the
//    FileWatcher directly because it is single-threaded)
//
// Enabled by default at construction. Application calls Disable() in standalone
// mode before Start(); every public method early-outs when disabled, so callers
// never need to branch on the mode themselves.
class HotReloader
{
public:
    // Moved to <ResourceManager/Core/ResourceUpload.h> so IResourceGpuSync can
    // name it without including this header. The alias keeps every existing
    // HotReloader::ReadyUpload call site (and t_HotReloader) compiling unchanged.
    using ReadyUpload = ResourceUpload;

    NOUS_ENGINE_API HotReloader(IImporterDispatcher* importerManager,
                                const TypeRegistry& typeRegistry,
                                nous::engine::multithreading::NOUS_JobSystem* jobSystem);

    // Disable hot reload (game/standalone mode). Must be called before Start();
    // after that, switching modes mid-session is not supported.
    NOUS_ENGINE_API void Disable() noexcept;
    [[nodiscard]] NOUS_ENGINE_API bool IsEnabled() const noexcept;

    // Resolves the source Assets/ directory (walks up from cwd looking for both
    // Assets/ and Source/ children) and registers every importable asset with
    // the FileWatcher. No-op when disabled.
    NOUS_ENGINE_API void Start();

    // Called from ModuleResourceManager::PreUpdate. Drains any watch
    // registrations queued from worker threads, then polls the watcher every
    // ~30 frames. No-op when disabled.
    NOUS_ENGINE_API void Poll();

    // Spin-waits until every in-flight reimport job has finished. Called from
    // ModuleResourceManager::CleanUp so worker lambdas (which capture `this`)
    // finish before the reloader is destroyed.
    NOUS_ENGINE_API void WaitForInFlight();

    // Records the (uid, type, assetsPath) association for a resource and
    // queues a watch registration to be picked up by the next Poll().
    // Safe to call from any thread. No-op when disabled, when assetsPath is
    // empty, or when the type is not hot-reloadable (currently TEXTURE/MATERIAL).
    NOUS_ENGINE_API void TrackAsset(const std::string& assetsPath, uint32_t uid, ResourceType type);

    // Removes any tracked entry for the given assets path. Safe to call from
    // any thread. No-op when disabled or when the path was never tracked.
    NOUS_ENGINE_API void UntrackAsset(const std::string& assetsPath);

    // Dispatches an async reimport job for a tracked asset path. Public to
    // allow direct testing; normally only called by the FileWatcher callback.
    NOUS_ENGINE_API void DispatchReimportJob(const std::string& normalizedPath);

    // Hands off the worker -> main-thread queue. Drained by
    // ModuleRenderer3D::PreUpdate each frame.
    NOUS_ENGINE_API std::vector<ReadyUpload> TakeReadyUploads();

    // Normalizes backslashes to forward slashes. Public utility — callers that
    // hand normalized paths back in (e.g. tests) can use this to stay
    // consistent with the form stored in m_tracked.
    NOUS_ENGINE_API static std::string NormalizeAssetPath(const std::string& path);

private:
    struct TrackedAsset
    {
        uint32_t       uid  = 0;
        ResourceType type = ResourceType::UNKNOWN;
        std::string  assetsPath;  // engine-relative form, e.g. "Assets/foo/bar.png"
    };

    // Walks up from cwd until a directory has both Assets/ and Source/ children.
    // Returns the absolute forward-slash path to its Assets/ child, or empty.
    static std::string FindSourceAssetsDir();

    // Filter shared by the initial scan and per-asset registration. True when
    // `extWithDot` matches any source extension of a TypeDescriptor whose
    // hotReloadable flag is set. Drives off the registry — adding a new
    // hot-reloadable type requires no edit here.
    bool IsHotReloadableExtension(const std::string& extWithDot) const;

    void ScanAndRegisterWatchedAssets();
    void RegisterWatchForAsset(const std::string& assetsPath);

    IImporterDispatcher*                           m_importerManager = nullptr;
    const TypeRegistry*                            m_typeRegistry     = nullptr;
    nous::engine::multithreading::NOUS_JobSystem*  m_jobSystem        = nullptr;

    bool                                           m_enabled            = true;
    std::string                                    m_sourceAssetsDir;
    uint32_t                                       m_pollFrameCounter   = 0;
    FileWatcher                                    m_watcher;

    // path (normalized, engine-relative) -> tracked record
    mutable std::mutex                             m_trackedMutex;
    std::unordered_map<std::string, TrackedAsset>  m_tracked;

    // dedup: at most one reimport job per UID at a time
    std::mutex                                     m_inFlightMutex;
    std::unordered_set<uint32_t>                     m_inFlightUIDs;
    std::atomic<int>                               m_inFlightJobCount { 0 };

    // worker -> main-thread handoff (drained by Renderer)
    std::mutex                                     m_readyMutex;
    std::vector<ReadyUpload>                       m_ready;

    // pending watcher registrations queued from worker threads (drained on Poll)
    std::mutex                                     m_pendingRegMutex;
    std::vector<std::string>                       m_pendingRegistrations;
};

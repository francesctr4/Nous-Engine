#pragma once

#include <ResourceManager/Core/ResourceUpload.h>

#include <cstdint>
#include <utility>
#include <vector>

class IGPUResourceFactory;
class IImporterManager;
class ResourceBase;

// -----------------------------------------------------------------------------
// The resource system, as seen by whoever owns the GPU.
// -----------------------------------------------------------------------------
//
// Implemented by ModuleResourceManager; the only consumer is ModuleRenderer3D,
// which drains these queues once per frame in PreUpdate and tears the whole
// registry down in CleanUp.
//
// Why this exists: ModuleRenderer3D used to hold a ModuleResourceManager* and
// therefore saw all ~22 of its exported methods -- ScanAndImportAssets,
// RegenerateLibrary, GetTypeRegistry, the scene manifest -- none of which mean
// anything to a renderer. Behind this interface it sees seven, and the module's
// asset-pipeline half is no longer reachable from the render loop at all.
//
// Deliberately NOT merged with the two interfaces the same module already
// implements, because the client sets do not overlap:
//   - IResourceLoader          create / request / unload, for components and importers
//   - IRenderResourceProvider  fallback resources + typed iteration, for Renderer/
//   - IResourceGpuSync         frame-scoped upload/release queues (this one)
// ModuleRenderer3D happens to use all three, which is what a module composing
// several systems looks like; that is not a reason to fuse them into one
// 20-method surface that every other consumer would then inherit.
//
// This header lives in Systems/ResourceManager/, next to IResourceLoader, rather
// than in Modules/: the resource system defines the port, the module implements
// it. An interface parked beside the implementation would buy nothing.
class IResourceGpuSync
{
public:
    virtual ~IResourceGpuSync() = default;

    // ───────────────────────────── Per-frame handoff ─────────────────────────
    // All three take-and-clear. Each is drained exactly once per frame, on the
    // main thread, and the ordering between them is a contract documented at
    // ModuleRenderer3D::PreUpdate -- uploads must run before reslots, which must
    // run before reload dispatch.

    /** @brief Resources deserialized on a worker and awaiting their first GPU upload. */
    virtual std::vector<std::pair<ResourceType, ResourceBase*>> TakePendingUploads() = 0;

    /** @brief Resources whose refcount hit zero, awaiting GPU release then CPU evict. */
    virtual std::vector<std::pair<ResourceType, ResourceBase*>> TakePendingReleases() = 0;

    /** @brief Hot-reloaded assets awaiting re-upload. Identified by UID, not
     *         pointer: the resource may have been evicted since the reimport
     *         finished, so the caller resolves it and skips a null result. */
    virtual std::vector<ResourceUpload> TakeReadyAssetUploads() = 0;

    // ───────────────────────────── Resolution ────────────────────────────────

    /** @brief Borrowed pointer for a loaded UID, or null. Does NOT bump the
     *         reference count -- never pair this with UnloadResource. */
    virtual ResourceBase* GetLoadedResource(uint32_t uid) = 0;

    /** @brief The importer manager, through which the caller drives Upload and
     *         Release for a given ResourceType. */
    [[nodiscard]] virtual IImporterManager* GetImporterManager() const = 0;

    // ───────────────────────────── Lifecycle ─────────────────────────────────

    /** @brief Releases the GPU handle for a drained release entry and evicts the
     *         CPU data. Returns false if the resource was re-acquired between
     *         being queued and being evicted, in which case it is re-queued for
     *         upload and must not be deleted. */
    virtual bool EvictResource(ResourceType type, ResourceBase* resource) = 0;

    /** @brief Synchronous full teardown. The caller passes the GPU factory so
     *         every GPU handle is freed while the device is still alive -- the
     *         renderer is torn down before the resource system, so this must be
     *         driven from ModuleRenderer3D::CleanUp, not from the module's own. */
    virtual void ClearResources(IGPUResourceFactory* gpu) = 0;
};

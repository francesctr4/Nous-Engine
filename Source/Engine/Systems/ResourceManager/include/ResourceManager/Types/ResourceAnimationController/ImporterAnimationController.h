#pragma once

#include <ResourceManager/Core/IImporter.h>
#include <EngineCore/EngineExport.h>

#include <string>

class ResourceAnimationController;

/**
 * @brief .nctrl importer. Data-only: no GPU residency.
 *
 * Unlike ImporterAudioGraph, Import does NOT copy the asset verbatim: a controller
 * references peer .nanim assets, so the Library copy must carry each clip's libraryPath
 * and uid -- an exported game ships no Assets/ and resolves from those alone. That makes
 * this the ImporterMaterial shape.
 *
 * REFERENCE-COUNTING HAZARD, on the record because it has shipped twice in this tree:
 * Deserialize runs on a LIVE resource during asset hot-reload, not only on a fresh one.
 * It must release what each slot previously held, UNCONDITIONALLY and AFTER acquiring the
 * new one. The `previous != new` guard is wrong and leaks in the common case, since
 * re-resolving finds the same clip resident and only increments.
 */
struct ImporterAnimationController : IResourceImporter
{
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;
    NOUS_ENGINE_API bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    NOUS_ENGINE_API bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;
    NOUS_ENGINE_API void Evict(ResourceBase* resource) override;
    NOUS_ENGINE_API bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    NOUS_ENGINE_API void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Bumping `generation` is the CALLER's job: the editor does it so a playing CAnimator
    // notices, and the round-trip test must not, so it can compare.
    static NOUS_ENGINE_API bool WriteControllerToFile(const ResourceAnimationController& controller,
                                                      const std::string& path);

    // Writes a minimal empty .nctrl at assetPath -- the editor's File > New.
    static NOUS_ENGINE_API bool CreateNewControllerFile(const std::string& assetPath);

private:
    // Acquires one ResourceAnimation per state and releases whatever the previous pass
    // held. Split out because the release-after-acquire ordering is the whole hazard.
    void ResolveClips(ResourceAnimationController* controller);
};

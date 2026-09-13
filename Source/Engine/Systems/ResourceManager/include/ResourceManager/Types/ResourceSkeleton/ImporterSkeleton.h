#pragma once

#include <AnimationSystem/Skeleton.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/IImporter.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nous::engine::resource_manager
{
    // FNV-1a over the bone names joined by '\n'. Stable across runs and platforms,
    // which a std::hash-based scheme would not be -- std::hash is allowed to be
    // salted per process, so it cannot go in a file.
    [[nodiscard]] NOUS_ENGINE_API uint64_t HashBoneNames(const std::vector<std::string>& names);
}

// Data-only importer: no GPU upload, mirroring ImporterAudioGraph.
struct ImporterSkeleton : IResourceImporter
{
    // FALLBACK PATH ONLY. Reads the .nskel stub's "source" model, re-parses it and
    // writes the binary.
    //
    // Normal imports never reach here: ImportModel parses a model once and calls
    // SaveSkeleton directly with the result. This fires when a stub outlives its
    // library binary -- a Library/ nuke, or RegenerateLibrary.
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;

    NOUS_ENGINE_API bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    NOUS_ENGINE_API bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;

    // Upload/Release are no-ops and Evict just drops the CPU data: a skeleton has
    // no GPU object, so a null IGPUResourceFactory is safe here.
    NOUS_ENGINE_API void Evict(ResourceBase* resource) override;
    NOUS_ENGINE_API bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    NOUS_ENGINE_API void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Writes an already-parsed SkeletonData to metaFileData.libraryPath.
    //
    // The entry point ImportModel uses, and the one the round-trip test drives --
    // tests go through the REAL writer so a fixture cannot silently stop describing
    // the format, which is what happened to t_ImporterMesh when Vertex3D grew.
    static NOUS_ENGINE_API bool SaveSkeleton(const MetaFileData& metaFileData,
                                             const nous::engine::animation_system::SkeletonData& skeleton);
};

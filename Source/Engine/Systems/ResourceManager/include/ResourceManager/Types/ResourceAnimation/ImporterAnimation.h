#pragma once

#include <AnimationSystem/AnimClip.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/IImporter.h>

#include <string>

// Data-only importer: no GPU upload, mirroring ImporterSkeleton.
struct ImporterAnimation : IResourceImporter
{
    // FALLBACK PATH ONLY -- see ImporterSkeleton::Import. Reads the .nanim stub's
    // "source" and "clip" fields, re-parses the model, and extracts that one clip by
    // its ORIGINAL name. That is why the stub stores the unsanitized name: the
    // filename has had its dots replaced ("mixamo.com" -> "mixamo_com") and could
    // not be matched back against the aiScene.
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;

    NOUS_ENGINE_API bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    NOUS_ENGINE_API bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;

    // Upload/Release are no-ops and Evict just drops the CPU data: a clip has no GPU
    // object, so a null IGPUResourceFactory is safe here.
    NOUS_ENGINE_API void Evict(ResourceBase* resource) override;
    NOUS_ENGINE_API bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    NOUS_ENGINE_API void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Writes an already-parsed clip. ImportModel's entry point, and the one the
    // round-trip test drives -- tests go through the REAL writer so a fixture cannot
    // silently stop describing the format.
    static NOUS_ENGINE_API bool SaveClip(const MetaFileData& metaFileData,
                                         const nous::engine::animation_system::AnimClipData& clip);
};

#pragma once

#include <AnimationSystem/Clip/AnimClip.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/IImporter.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>   // AnimationSettings

#include <string>

// Data-only importer: no GPU upload, mirroring ImporterSkeleton.
struct ImporterAnimation : IResourceImporter
{
    // FALLBACK PATH ONLY -- see ImporterSkeleton::Import. Reads the stub's "source" and
    // "clip" fields, re-parses the model and extracts that one clip by its ORIGINAL name.
    // That is why the stub stores the unsanitized name: the filename has had its dots
    // replaced ("mixamo.com" -> "mixamo_com") and cannot be matched back against the file.
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;

    NOUS_ENGINE_API bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    NOUS_ENGINE_API bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;

    // Upload/Release are no-ops and Evict just drops the CPU data: a clip has no GPU
    // object, so a null IGPUResourceFactory is safe here.
    NOUS_ENGINE_API void Evict(ResourceBase* resource) override;
    NOUS_ENGINE_API bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    NOUS_ENGINE_API void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Writes an already-parsed clip: ImportModel's entry point, and what the round-trip
    // test drives, so a fixture cannot silently stop describing the format.
    //
    // `authoring` is REQUIRED rather than defaulted: every caller has to decide where the
    // authored values come from, and ImportModel's answer is ReadAuthoringFromStub. A
    // default would silently reset the user's settings and markers on every re-import.
    static NOUS_ENGINE_API bool SaveClip(const MetaFileData& metaFileData,
                                         const nous::engine::animation_system::AnimClipData& clip,
                                         const ClipAuthoring& authoring);

    // The stub is the authoring copy -- the one thing that survives a Library/ nuke, since
    // EnsureStub never overwrites an existing stub while the binary is regenerated on every
    // import. An unedited stub declares none of these keys, so missing means DEFAULT.
    static NOUS_ENGINE_API ClipAuthoring ReadAuthoringFromStub(const std::string& stubPath);

    // Read-modify-write, so the "source"/"clip" keys the fallback re-parse depends on
    // survive.
    static NOUS_ENGINE_API bool WriteAuthoringToStub(const std::string& stubPath,
                                                     const ClipAuthoring& authoring);

    // What the editor calls when a per-clip control changes: pushes settings AND events
    // into BOTH copies. One call rather than two because the writes are only correct
    // together -- a stub and a binary that disagree stay that way until the next re-import
    // silently picks a winner.
    static NOUS_ENGINE_API bool SaveAuthoring(ResourceAnimation& animation);
};

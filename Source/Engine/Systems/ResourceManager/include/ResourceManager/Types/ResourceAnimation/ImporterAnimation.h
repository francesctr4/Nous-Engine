#pragma once

#include <AnimationSystem/AnimClip.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/IImporter.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>   // AnimationSettings

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
    //
    // `settings` is REQUIRED rather than defaulted on purpose: the binary is the copy
    // an exported game ships, so every caller has to decide where the authored values
    // come from. ImportModel's answer is ReadSettingsFromStub -- a default there would
    // silently reset the user's edits on every re-import.
    static NOUS_ENGINE_API bool SaveClip(const MetaFileData& metaFileData,
                                         const nous::engine::animation_system::AnimClipData& clip,
                                         const AnimationSettings& settings);

    // The .nanim STUB is the authoring copy: it is the one thing that survives a
    // Library/ nuke, since EnsureStub never overwrites an existing stub while the
    // binary is regenerated on every import.
    //
    // A stub that has never been edited declares neither key -- EnsureStub writes only
    // "source" and "clip" -- so missing means DEFAULT, not corrupt.
    static NOUS_ENGINE_API AnimationSettings ReadSettingsFromStub(const std::string& stubPath);

    // Read-modify-write, so the "source"/"clip" keys the fallback re-parse depends on
    // survive. The editor pairs this with Save() to keep stub and binary in sync on
    // the tick of a checkbox -- the WriteAudioGraphToFile precedent.
    static NOUS_ENGINE_API bool WriteSettingsToStub(const std::string& stubPath,
                                                    const AnimationSettings& settings);

    // What the editor calls when a per-clip control changes: pushes the resource's
    // settings into BOTH copies at once. One call rather than two because the two
    // writes are only correct together -- a stub and a binary that disagree stay
    // that way until the next re-import silently picks a winner.
    static NOUS_ENGINE_API bool SaveSettings(ResourceAnimation& animation);
};

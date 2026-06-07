#pragma once

#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/EngineExport.h"

#include <string>

class ResourceAudioGraph;

// Data-only importer (mirrors ImporterAudio): no GPU upload. Import copies the
// authored .nafx into Library/ verbatim (no external asset refs to enrich);
// Deserialize parses effects + editor block into a ResourceAudioGraph.
struct ImporterAudioGraph : IResourceImporter
{
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;
    NOUS_ENGINE_API bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    NOUS_ENGINE_API bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;
    NOUS_ENGINE_API void Evict(ResourceBase* resource) override;
    NOUS_ENGINE_API bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    NOUS_ENGINE_API void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;

    // Writes a ResourceAudioGraph to a .nafx JSON file (effects + editor block).
    // Used by the round-trip test now and by the AudioGraphEditor's Save (Layer 3).
    static NOUS_ENGINE_API bool WriteAudioGraphToFile(const ResourceAudioGraph& graph,
                                                      const std::string& path);

    // Writes a minimal empty .nafx (no effects) at assetPath — the editor's
    // "New" flow (Layer 3).
    static NOUS_ENGINE_API bool CreateNewAudioGraphFile(const std::string& assetPath);
};

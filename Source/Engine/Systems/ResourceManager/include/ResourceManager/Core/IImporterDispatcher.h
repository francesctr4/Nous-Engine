#pragma once

#include <cstdint>

enum class ResourceType : int8_t;
struct MetaFileData;

// Import-only seam: dispatches an asset import to the registered importer for a
// resource type. This is the entire surface the asset pipeline needs —
// ImportPipeline and HotReloader depend on this and never touch the runtime
// resource lifecycle (Deserialize / Evict / Upload / Release), which lives on
// IImporterManager. Splitting it out lets their test mocks stub a single method
// instead of the full manager interface.
class IImporterDispatcher
{
public:
    virtual ~IImporterDispatcher() = default;

    virtual bool Import(ResourceType type, const MetaFileData& metaFileData) = 0;
};

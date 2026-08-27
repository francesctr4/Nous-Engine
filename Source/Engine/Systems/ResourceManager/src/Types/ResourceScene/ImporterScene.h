#pragma once

#include "Engine/EngineExport.h"
#include <ResourceManager/Core/IImporter.h>

// Scene importer — scenes have no runtime resource object (no `ResourceScene`
// class) and no GPU residency, so this is a pipeline-only IAssetImporter: the
// only work to do at import time is copy the source `.nous` from Assets/ to its
// UID-keyed Library/Scenes/ destination, matching how every other library asset
// is laid out. There is no CPU/GPU lifecycle to implement — scene loading goes
// through ModuleScene, not the generic Resource lifecycle.
class ImporterScene final : public IAssetImporter
{
public:
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;
};

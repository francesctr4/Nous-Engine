#include "Engine/Systems/ResourceManager/Types/ResourceScene/include/ImporterScene.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/MetaFileData.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

bool ImporterScene::Import(const MetaFileData& metaFileData)
{
    if (!nous::engine::filesystem::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "[ImporterScene] Failed to mirror '%s' -> '%s'.",
            metaFileData.assetsPath.c_str(), metaFileData.libraryPath.c_str());
        return false;
    }
    return true;
}
